#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")

using namespace DirectX;

template<class T> static void rel(T*& p){ if(p){ p->Release(); p=nullptr; } }
struct Vertex { XMFLOAT3 p,n; XMFLOAT2 uv; };
struct CB { XMMATRIX world,view,proj; XMFLOAT4 color; XMFLOAT3 lightDir; float useTex; };

static HWND g_hwnd=nullptr; static int g_w=1440,g_h=900; static bool g_keys[256]{}; static float g_zoom=1.0f;
static ID3D11Device* g_dev=nullptr; static ID3D11DeviceContext* g_ctx=nullptr; static IDXGISwapChain* g_swap=nullptr;
static ID3D11RenderTargetView* g_rtv=nullptr; static ID3D11Texture2D* g_depth=nullptr; static ID3D11DepthStencilView* g_dsv=nullptr;
static ID3D11VertexShader* g_vs=nullptr; static ID3D11PixelShader* g_ps=nullptr; static ID3D11InputLayout* g_layout=nullptr;
static ID3D11Buffer *g_cb=nullptr,*g_terrainVB=nullptr,*g_terrainIB=nullptr,*g_blobVB=nullptr,*g_blobIB=nullptr;
static ID3D11ShaderResourceView* g_groundSRV=nullptr; static ID3D11SamplerState* g_sampler=nullptr;
static UINT g_terrainIndexCount=0,g_blobIndexCount=0;

static float g_px=0,g_pz=0,g_vx=0,g_vz=0,g_flow=0;
static const int BLAT=24, BLON=48; static std::vector<XMFLOAT3> g_blobLocal; static std::vector<Vertex> g_blobVerts;

static float hashf(float x,float z){ float v=sinf(x*12.9898f+z*78.233f)*43758.5453f; return v-floorf(v); }
static float groundH(float x,float z){
    float h=0.0f;
    h += sinf(x*.085f)*1.1f + cosf(z*.072f)*.9f;
    h += sinf((x+z)*.19f)*.35f;
    h += (hashf(floorf(x*.35f),floorf(z*.35f))-.5f)*.12f;
    return h*.42f;
}
static XMFLOAT3 terrainNormal(float x,float z){ float e=.15f; float dx=groundH(x+e,z)-groundH(x-e,z), dz=groundH(x,z+e)-groundH(x,z-e); XMVECTOR n=XMVector3Normalize(XMVectorSet(-dx,2*e,-dz,0)); XMFLOAT3 o; XMStoreFloat3(&o,n); return o; }

static bool fileExists(const std::wstring& p){ DWORD a=GetFileAttributesW(p.c_str()); return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY); }
static std::wstring findGroundTexture(){
    const wchar_t* bases[]={L"ExternalAssets\\PolyHaven\\forest_ground_04\\asset",L"ExternalAssets\\PolyHaven\\rocky_terrain_02\\asset",L"ExternalAssets\\PolyHaven\\mud_forest\\asset"};
    const wchar_t* exts[]={L".jpg",L".jpeg",L".png",L".bmp"};
    for(auto b:bases) for(auto e:exts){ std::wstring p=b; p+=e; if(fileExists(p)) return p; }
    return L"";
}

static bool loadTextureWIC(const std::wstring& path, ID3D11ShaderResourceView** out){
    *out=nullptr; IWICImagingFactory* f=nullptr; IWICBitmapDecoder* dec=nullptr; IWICBitmapFrameDecode* frame=nullptr; IWICFormatConverter* conv=nullptr;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f)))) return false;
    bool ok=false;
    if(SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&dec)) &&
       SUCCEEDED(dec->GetFrame(0,&frame)) && SUCCEEDED(f->CreateFormatConverter(&conv)) &&
       SUCCEEDED(conv->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0.0,WICBitmapPaletteTypeCustom))){
        UINT w=0,h=0; conv->GetSize(&w,&h); std::vector<uint8_t> px((size_t)w*h*4);
        if(SUCCEEDED(conv->CopyPixels(nullptr,w*4,(UINT)px.size(),px.data()))){
            D3D11_TEXTURE2D_DESC td{}; td.Width=w; td.Height=h; td.MipLevels=1; td.ArraySize=1; td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; td.SampleDesc.Count=1; td.Usage=D3D11_USAGE_DEFAULT; td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem=px.data(); sd.SysMemPitch=w*4; ID3D11Texture2D* t=nullptr;
            if(SUCCEEDED(g_dev->CreateTexture2D(&td,&sd,&t))){ D3D11_SHADER_RESOURCE_VIEW_DESC sv{}; sv.Format=td.Format; sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D; sv.Texture2D.MipLevels=1; ok=SUCCEEDED(g_dev->CreateShaderResourceView(t,&sv,out)); t->Release(); }
        }
    }
    rel(conv); rel(frame); rel(dec); rel(f); return ok;
}

static bool compileShader(const char* src,const char* entry,const char* target,ID3DBlob** blob){
    ID3DBlob* err=nullptr; HRESULT hr=D3DCompile(src,strlen(src),nullptr,nullptr,nullptr,entry,target,D3DCOMPILE_ENABLE_STRICTNESS,0,blob,&err);
    if(FAILED(hr) && err){ OutputDebugStringA((const char*)err->GetBufferPointer()); MessageBoxA(g_hwnd,(const char*)err->GetBufferPointer(),"Shader compile error",MB_ICONERROR); }
    rel(err); return SUCCEEDED(hr);
}

static void destroySize(){ rel(g_rtv); rel(g_dsv); rel(g_depth); }
static bool createSize(){
    destroySize(); ID3D11Texture2D* bb=nullptr; if(FAILED(g_swap->GetBuffer(0,IID_PPV_ARGS(&bb)))) return false;
    HRESULT hr=g_dev->CreateRenderTargetView(bb,nullptr,&g_rtv); bb->Release(); if(FAILED(hr)) return false;
    D3D11_TEXTURE2D_DESC dd{}; dd.Width=g_w;dd.Height=g_h;dd.MipLevels=1;dd.ArraySize=1;dd.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;dd.SampleDesc.Count=1;dd.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    if(FAILED(g_dev->CreateTexture2D(&dd,nullptr,&g_depth)))return false; if(FAILED(g_dev->CreateDepthStencilView(g_depth,nullptr,&g_dsv)))return false; return true;
}

static bool initD3D(){
    DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount=2;sd.BufferDesc.Width=g_w;sd.BufferDesc.Height=g_h;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=g_hwnd;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl; UINT flags=0;
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,&g_swap,&g_dev,&fl,&g_ctx))) return false;
    if(!createSize()) return false;
    const char* vs=R"(
cbuffer C:register(b0){matrix world;matrix view;matrix proj;float4 color;float3 lightDir;float useTex;}
struct I{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;}; struct O{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float3 wp:TEXCOORD1;};
O main(I i){O o;float4 wp=mul(float4(i.p,1),world);o.wp=wp.xyz;o.p=mul(mul(wp,view),proj);o.n=normalize(mul(float4(i.n,0),world).xyz);o.uv=i.uv;return o;})";
    const char* ps=R"(
cbuffer C:register(b0){matrix world;matrix view;matrix proj;float4 color;float3 lightDir;float useTex;} Texture2D tex0:register(t0);SamplerState samp0:register(s0);
struct O{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float3 wp:TEXCOORD1;};
float4 main(O i):SV_TARGET{float3 n=normalize(i.n);float nd=max(.12,dot(n,normalize(-lightDir)));float3 base=color.rgb;if(useTex>.5)base*=tex0.Sample(samp0,i.uv).rgb;float rim=pow(1-saturate(dot(n,normalize(float3(.25,.75,-.55)))),2)*.10;return float4(base*(.28+.82*nd)+rim,1);})";
    ID3DBlob *bvs=nullptr,*bps=nullptr; if(!compileShader(vs,"main","vs_5_0",&bvs)||!compileShader(ps,"main","ps_5_0",&bps))return false;
    if(FAILED(g_dev->CreateVertexShader(bvs->GetBufferPointer(),bvs->GetBufferSize(),nullptr,&g_vs)))return false;
    if(FAILED(g_dev->CreatePixelShader(bps->GetBufferPointer(),bps->GetBufferSize(),nullptr,&g_ps)))return false;
    D3D11_INPUT_ELEMENT_DESC il[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(g_dev->CreateInputLayout(il,3,bvs->GetBufferPointer(),bvs->GetBufferSize(),&g_layout)))return false; rel(bvs); rel(bps);
    D3D11_BUFFER_DESC cbd{};cbd.ByteWidth=sizeof(CB);cbd.Usage=D3D11_USAGE_DEFAULT;cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER; if(FAILED(g_dev->CreateBuffer(&cbd,nullptr,&g_cb)))return false;
    D3D11_SAMPLER_DESC ss{};ss.Filter=D3D11_FILTER_ANISOTROPIC;ss.AddressU=ss.AddressV=ss.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;ss.MaxAnisotropy=8;ss.MaxLOD=D3D11_FLOAT32_MAX;if(FAILED(g_dev->CreateSamplerState(&ss,&g_sampler)))return false;
    std::wstring p=findGroundTexture(); if(p.empty() || !loadTextureWIC(p,&g_groundSRV)){ MessageBoxW(g_hwnd,L"No se pudo cargar la textura CC0 del terreno. Ejecuta build_sp.cmd para descargar los assets.",L"SPVersion",MB_ICONERROR); return false; }
    return true;
}

static void buildTerrain(){
    const int N=129; const float SIZE=96.f, step=SIZE/(N-1), half=SIZE*.5f; std::vector<Vertex> v; v.reserve(N*N);
    for(int z=0;z<N;++z)for(int x=0;x<N;++x){float wx=-half+x*step,wz=-half+z*step;Vertex a{};a.p={wx,groundH(wx,wz),wz};a.n=terrainNormal(wx,wz);a.uv={wx*.085f,wz*.085f};v.push_back(a);} std::vector<uint32_t> ind;ind.reserve((N-1)*(N-1)*6);
    for(int z=0;z<N-1;++z)for(int x=0;x<N-1;++x){uint32_t i=z*N+x;ind.insert(ind.end(),{i,i+N,i+1,i+1,i+N,i+N+1});}g_terrainIndexCount=(UINT)ind.size();
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=(UINT)(v.size()*sizeof(Vertex));bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=v.data();g_dev->CreateBuffer(&bd,&sd,&g_terrainVB);
    bd.ByteWidth=(UINT)(ind.size()*sizeof(uint32_t));bd.BindFlags=D3D11_BIND_INDEX_BUFFER;sd.pSysMem=ind.data();g_dev->CreateBuffer(&bd,&sd,&g_terrainIB);
}

static void buildBlob(){
    g_blobLocal.clear();g_blobVerts.clear();std::vector<uint32_t> ind;
    const float rx=2.15f,ry=1.25f,rz=1.85f;
    for(int la=0;la<=BLAT;++la){float th=XM_PI*la/BLAT;for(int lo=0;lo<=BLON;++lo){float ph=XM_2PI*lo/BLON;float s=sinf(th);XMFLOAT3 q={rx*s*cosf(ph),ry*cosf(th),rz*s*sinf(ph)};float lump=1.f+.055f*sinf(ph*3.f+th*2.f)+.035f*cosf(ph*5.f-th);q.x*=lump;q.y*=lump;q.z*=lump;g_blobLocal.push_back(q);Vertex a{};a.p=q;a.n={q.x/(rx*rx),q.y/(ry*ry),q.z/(rz*rz)};XMVECTOR n=XMVector3Normalize(XMLoadFloat3(&a.n));XMStoreFloat3(&a.n,n);a.uv={(float)lo/BLON,(float)la/BLAT};g_blobVerts.push_back(a);}}
    for(int la=0;la<BLAT;++la)for(int lo=0;lo<BLON;++lo){uint32_t i=la*(BLON+1)+lo;ind.insert(ind.end(),{i,i+BLON+1,i+1,i+1,i+BLON+1,i+BLON+2});}g_blobIndexCount=(UINT)ind.size();
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=(UINT)(g_blobVerts.size()*sizeof(Vertex));bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;g_dev->CreateBuffer(&bd,nullptr,&g_blobVB);
    bd={};bd.ByteWidth=(UINT)(ind.size()*sizeof(uint32_t));bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_INDEX_BUFFER;D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=ind.data();g_dev->CreateBuffer(&bd,&sd,&g_blobIB);
}

static XMFLOAT3 rotateAxis(const XMFLOAT3& p,const XMFLOAT3& a,float ang){XMVECTOR P=XMLoadFloat3(&p),A=XMVector3Normalize(XMLoadFloat3(&a));XMMATRIX R=XMMatrixRotationAxis(A,ang);XMFLOAT3 o;XMStoreFloat3(&o,XMVector3TransformNormal(P,R));return o;}
static void update(float dt){
    float ix=0,iz=0;if(g_keys['A']||g_keys[VK_LEFT])ix-=1;if(g_keys['D']||g_keys[VK_RIGHT])ix+=1;if(g_keys['W']||g_keys[VK_UP])iz+=1;if(g_keys['S']||g_keys[VK_DOWN])iz-=1;float m=sqrtf(ix*ix+iz*iz);if(m>0){ix/=m;iz/=m;}float ramp=1-expf(-7.f*dt);g_flow+=(m-g_flow)*ramp;float target=4.0f*g_flow;float tvx=ix*target,tvz=iz*target;g_vx+=(tvx-g_vx)*(1-expf(-5.5f*dt));g_vz+=(tvz-g_vz)*(1-expf(-5.5f*dt));if(m==0){float d=expf(-4.f*dt);g_vx*=d;g_vz*=d;}g_px+=g_vx*dt;g_pz+=g_vz*dt;g_px=(std::max)(-42.f,(std::min)(42.f,g_px));g_pz=(std::max)(-42.f,(std::min)(42.f,g_pz));
    float sp=sqrtf(g_vx*g_vx+g_vz*g_vz);if(sp>.01f){XMFLOAT3 axis={g_vz,0,-g_vx};float ang=(sp/1.75f)*dt;for(auto& q:g_blobLocal)q=rotateAxis(q,axis,ang);}for(size_t i=0;i<g_blobVerts.size();++i){auto q=g_blobLocal[i];float floor=groundH(g_px+q.x,g_pz+q.z)-groundH(g_px,g_pz);if(q.y<floor-.95f)q.y=floor-.95f;g_blobVerts[i].p=q;XMVECTOR n=XMVector3Normalize(XMVectorSet(q.x/4.6f,q.y/1.55f,q.z/3.4f,0));XMStoreFloat3(&g_blobVerts[i].n,n);}D3D11_MAPPED_SUBRESOURCE ms{};if(SUCCEEDED(g_ctx->Map(g_blobVB,0,D3D11_MAP_WRITE_DISCARD,0,&ms))){memcpy(ms.pData,g_blobVerts.data(),g_blobVerts.size()*sizeof(Vertex));g_ctx->Unmap(g_blobVB,0);}
}

static void drawMesh(ID3D11Buffer* vb,ID3D11Buffer* ib,UINT count,const XMMATRIX& world,const XMFLOAT4& color,float useTex){
    UINT stride=sizeof(Vertex),off=0;g_ctx->IASetVertexBuffers(0,1,&vb,&stride,&off);g_ctx->IASetIndexBuffer(ib,DXGI_FORMAT_R32_UINT,0);g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    XMVECTOR eye=XMVectorSet(g_px-13.f*g_zoom,10.f*g_zoom,g_pz-15.f*g_zoom,1),at=XMVectorSet(g_px,groundH(g_px,g_pz)+.4f,g_pz,1),up=XMVectorSet(0,1,0,0);XMMATRIX view=XMMatrixLookAtLH(eye,at,up),proj=XMMatrixPerspectiveFovLH(XMConvertToRadians(49.f),g_w/(float)g_h,.1f,180.f);
    CB cb{};cb.world=XMMatrixTranspose(world);cb.view=XMMatrixTranspose(view);cb.proj=XMMatrixTranspose(proj);cb.color=color;cb.lightDir={-.45f,-1.f,.35f};cb.useTex=useTex;g_ctx->UpdateSubresource(g_cb,0,nullptr,&cb,0,0);g_ctx->VSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetShaderResources(0,1,&g_groundSRV);g_ctx->PSSetSamplers(0,1,&g_sampler);g_ctx->DrawIndexed(count,0,0);
}
static void render(){
    float clear[]={.055f,.065f,.052f,1};g_ctx->OMSetRenderTargets(1,&g_rtv,g_dsv);g_ctx->ClearRenderTargetView(g_rtv,clear);g_ctx->ClearDepthStencilView(g_dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);D3D11_VIEWPORT vp={0,0,(float)g_w,(float)g_h,0,1};g_ctx->RSSetViewports(1,&vp);g_ctx->IASetInputLayout(g_layout);g_ctx->VSSetShader(g_vs,nullptr,0);g_ctx->PSSetShader(g_ps,nullptr,0);
    drawMesh(g_terrainVB,g_terrainIB,g_terrainIndexCount,XMMatrixIdentity(),{.78f,.78f,.78f,1},1.f);
    float by=groundH(g_px,g_pz)+1.18f;drawMesh(g_blobVB,g_blobIB,g_blobIndexCount,XMMatrixTranslation(g_px,by,g_pz),{.42f,.055f,.075f,1},0.f);
    g_swap->Present(1,0);
}

static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED){g_w=LOWORD(l);g_h=HIWORD(l);destroySize();g_swap->ResizeBuffers(0,g_w,g_h,DXGI_FORMAT_UNKNOWN,0);createSize();}return 0;case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(h,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;case WM_MOUSEWHEEL:g_zoom*=GET_WHEEL_DELTA_WPARAM(w)>0?.90f:1.11f;g_zoom=(std::max)(.55f,(std::min)(2.4f,g_zoom));return 0;case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w==VK_ESCAPE)DestroyWindow(h);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(bool&k:g_keys)k=false;return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);WNDCLASSW wc{};wc.hInstance=hi;wc.lpfnWndProc=wnd;wc.lpszClassName=L"SP_D3D11";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,80,60,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;if(!initD3D())return 2;buildTerrain();buildBlob();LARGE_INTEGER f,p,n;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&p);MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}QueryPerformanceCounter(&n);float dt=(float)((double)(n.QuadPart-p.QuadPart)/f.QuadPart);p=n;dt=(std::min)(dt,.033f);update(dt);render();}rel(g_sampler);rel(g_groundSRV);rel(g_blobIB);rel(g_blobVB);rel(g_terrainIB);rel(g_terrainVB);rel(g_cb);rel(g_layout);rel(g_ps);rel(g_vs);destroySize();rel(g_swap);rel(g_ctx);rel(g_dev);CoUninitialize();return 0;}
