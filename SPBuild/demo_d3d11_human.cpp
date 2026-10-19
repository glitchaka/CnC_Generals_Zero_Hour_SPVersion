#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")

using namespace DirectX;

template<class T> static void rel(T*& p){ if(p){ p->Release(); p=nullptr; } }
static float clampf(float v,float a,float b){ return (std::max)(a,(std::min)(b,v)); }
static float angleWrap(float a){ while(a>XM_PI)a-=XM_2PI; while(a<-XM_PI)a+=XM_2PI; return a; }
static float approachAngle(float a,float b,float step){ float d=angleWrap(b-a); if(fabsf(d)<=step)return b; return angleWrap(a+(d>0?step:-step)); }

struct Vertex { XMFLOAT3 p,n; XMFLOAT2 uv; };
struct MeshGPU { ID3D11Buffer* vb=nullptr; ID3D11Buffer* ib=nullptr; UINT count=0; XMFLOAT3 bmin{0,0,0},bmax{0,0,0}; };
struct SceneObj { MeshGPU* mesh=nullptr; float x=0,z=0,yaw=0,scale=1; bool collider=true; };
struct CB {
    XMMATRIX world,view,proj;
    XMFLOAT4 color;
    XMFLOAT3 lightDir; float useTex;
    XMFLOAT3 cameraPos; float fogStart;
    XMFLOAT3 fogColor; float fogEnd;
};

static HWND g_hwnd=nullptr;
static int g_w=1280,g_h=800;
static bool g_keys[256]{};
static bool g_lmb=false;
static POINT g_lastMouse{};
static float g_camYaw=.78f,g_camPitch=.58f,g_zoom=1.f;
static XMFLOAT3 g_camTarget{0,1,0},g_camEye{0,8,-10};

static ID3D11Device* g_dev=nullptr;
static ID3D11DeviceContext* g_ctx=nullptr;
static IDXGISwapChain* g_swap=nullptr;
static ID3D11RenderTargetView* g_rtv=nullptr;
static ID3D11Texture2D* g_depth=nullptr;
static ID3D11DepthStencilView* g_dsv=nullptr;
static ID3D11VertexShader* g_vs=nullptr;
static ID3D11PixelShader* g_ps=nullptr;
static ID3D11InputLayout* g_layout=nullptr;
static ID3D11Buffer* g_cb=nullptr;
static ID3D11SamplerState* g_sampler=nullptr;
static ID3D11ShaderResourceView* g_groundSRV=nullptr;
static ID3D11ShaderResourceView* g_kaySRV=nullptr;
static MeshGPU g_terrain,g_rubble,g_crates,g_barrel;
static std::vector<SceneObj> g_scene;

struct UIVertex { XMFLOAT2 p,uv; XMFLOAT4 color; };
static ID3D11VertexShader* g_uiVS=nullptr;
static ID3D11PixelShader* g_uiPS=nullptr;
static ID3D11InputLayout* g_uiLayout=nullptr;
static ID3D11Buffer* g_uiVB=nullptr;
static ID3D11BlendState* g_uiBlend=nullptr;
static ID3D11DepthStencilState* g_uiDepthOff=nullptr;
static ID3D11ShaderResourceView* g_uiPanel=nullptr;
static ID3D11ShaderResourceView* g_uiHealth=nullptr;
static ID3D11ShaderResourceView* g_uiSlot=nullptr;
static ID3D11ShaderResourceView* g_uiWhite=nullptr;

static bool fileExists(const std::wstring& p){ DWORD a=GetFileAttributesW(p.c_str()); return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY); }
static std::wstring findGroundTexture(){
    const wchar_t* roots[]={L"ExternalAssets\\PolyHaven\\forest_ground_04\\asset",L"ExternalAssets\\PolyHaven\\rocky_terrain_02\\asset",L"ExternalAssets\\PolyHaven\\mud_forest\\asset"};
    const wchar_t* ext[]={L".jpg",L".jpeg",L".png",L".bmp"};
    for(auto r:roots)for(auto e:ext){ std::wstring p=r; p+=e; if(fileExists(p))return p; }
    return L"";
}

static bool loadTextureWIC(const std::wstring& path,ID3D11ShaderResourceView** out){
    *out=nullptr;
    IWICImagingFactory* f=nullptr; IWICBitmapDecoder* d=nullptr; IWICBitmapFrameDecode* fr=nullptr; IWICFormatConverter* c=nullptr;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))))return false;
    bool ok=false;
    if(SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&d)) &&
       SUCCEEDED(d->GetFrame(0,&fr)) && SUCCEEDED(f->CreateFormatConverter(&c)) &&
       SUCCEEDED(c->Initialize(fr,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0.0,WICBitmapPaletteTypeCustom))){
        UINT w=0,h=0; c->GetSize(&w,&h); std::vector<uint8_t> px((size_t)w*h*4);
        if(SUCCEEDED(c->CopyPixels(nullptr,w*4,(UINT)px.size(),px.data()))){
            D3D11_TEXTURE2D_DESC td{}; td.Width=w; td.Height=h; td.MipLevels=1; td.ArraySize=1; td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; td.SampleDesc.Count=1; td.Usage=D3D11_USAGE_DEFAULT; td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem=px.data(); sd.SysMemPitch=w*4; ID3D11Texture2D* t=nullptr;
            if(SUCCEEDED(g_dev->CreateTexture2D(&td,&sd,&t))){ ok=SUCCEEDED(g_dev->CreateShaderResourceView(t,nullptr,out)); t->Release(); }
        }
    }
    rel(c); rel(fr); rel(d); rel(f); return ok;
}

static bool makeWhiteTexture(){
    uint32_t px=0xffffffffu; D3D11_TEXTURE2D_DESC td{}; td.Width=td.Height=1; td.MipLevels=td.ArraySize=1; td.Format=DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count=1; td.Usage=D3D11_USAGE_DEFAULT; td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem=&px; sd.SysMemPitch=4; ID3D11Texture2D* t=nullptr; if(FAILED(g_dev->CreateTexture2D(&td,&sd,&t)))return false; bool ok=SUCCEEDED(g_dev->CreateShaderResourceView(t,nullptr,&g_uiWhite)); t->Release(); return ok;
}

static bool compileShader(const char* src,const char* entry,const char* target,ID3DBlob** out){
    ID3DBlob* err=nullptr; HRESULT hr=D3DCompile(src,strlen(src),nullptr,nullptr,nullptr,entry,target,D3DCOMPILE_ENABLE_STRICTNESS,0,out,&err);
    if(FAILED(hr)&&err)MessageBoxA(g_hwnd,(const char*)err->GetBufferPointer(),"Shader",MB_ICONERROR); rel(err); return SUCCEEDED(hr);
}

static void destroySize(){ if(g_ctx)g_ctx->OMSetRenderTargets(0,nullptr,nullptr); rel(g_rtv); rel(g_dsv); rel(g_depth); }
static bool createSize(){
    if(!g_swap||g_w<1||g_h<1)return false; destroySize(); ID3D11Texture2D* bb=nullptr;
    if(FAILED(g_swap->GetBuffer(0,IID_PPV_ARGS(&bb))))return false; HRESULT hr=g_dev->CreateRenderTargetView(bb,nullptr,&g_rtv); bb->Release(); if(FAILED(hr))return false;
    D3D11_TEXTURE2D_DESC dd{}; dd.Width=g_w; dd.Height=g_h; dd.MipLevels=dd.ArraySize=1; dd.Format=DXGI_FORMAT_D24_UNORM_S8_UINT; dd.SampleDesc.Count=1; dd.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    if(FAILED(g_dev->CreateTexture2D(&dd,nullptr,&g_depth)))return false; return SUCCEEDED(g_dev->CreateDepthStencilView(g_depth,nullptr,&g_dsv));
}
static void resizeSwap(int w,int h){ if(!g_swap||w<1||h<1)return; g_w=w; g_h=h; destroySize(); if(SUCCEEDED(g_swap->ResizeBuffers(0,g_w,g_h,DXGI_FORMAT_UNKNOWN,0)))createSize(); }
static void syncClientSize(){ RECT r{}; GetClientRect(g_hwnd,&r); int w=r.right-r.left,h=r.bottom-r.top; if(w>0&&h>0&&(w!=g_w||h!=g_h))resizeSwap(w,h); }

static bool initD3D(){
    DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount=2; sd.BufferDesc.Width=g_w; sd.BufferDesc.Height=g_h; sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow=g_hwnd; sd.SampleDesc.Count=1; sd.Windowed=TRUE; sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl{}; if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&g_swap,&g_dev,&fl,&g_ctx)))return false; if(!createSize())return false;
    const char* vs=R"(cbuffer C:register(b0){matrix world;matrix view;matrix proj;float4 color;float3 lightDir;float useTex;float3 cameraPos;float fogStart;float3 fogColor;float fogEnd;}struct I{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;};struct O{float4 p:SV_POSITION;float3 wp:TEXCOORD1;float3 n:NORMAL;float2 uv:TEXCOORD0;};O main(I i){O o;float4 wp=mul(float4(i.p,1),world);o.wp=wp.xyz;o.p=mul(mul(wp,view),proj);o.n=normalize(mul(float4(i.n,0),world).xyz);o.uv=i.uv;return o;})";
    const char* ps=R"(cbuffer C:register(b0){matrix world;matrix view;matrix proj;float4 color;float3 lightDir;float useTex;float3 cameraPos;float fogStart;float3 fogColor;float fogEnd;}Texture2D tex0:register(t0);SamplerState samp0:register(s0);struct O{float4 p:SV_POSITION;float3 wp:TEXCOORD1;float3 n:NORMAL;float2 uv:TEXCOORD0;};float4 main(O i):SV_TARGET{float3 n=normalize(i.n);float sun=saturate(dot(n,normalize(-lightDir)));float sky=saturate(.5+.5*n.y);float3 base=color.rgb;if(useTex>.5)base*=tex0.Sample(samp0,i.uv).rgb;float lit=.32+.62*sun+.18*sky;float3 col=base*lit;float d=distance(cameraPos,i.wp);float f=saturate((d-fogStart)/max(.01,fogEnd-fogStart));col=lerp(col,fogColor,f);return float4(col,1);})";
    ID3DBlob* bv=nullptr,*bp=nullptr; if(!compileShader(vs,"main","vs_5_0",&bv)||!compileShader(ps,"main","ps_5_0",&bp))return false;
    if(FAILED(g_dev->CreateVertexShader(bv->GetBufferPointer(),bv->GetBufferSize(),nullptr,&g_vs))||FAILED(g_dev->CreatePixelShader(bp->GetBufferPointer(),bp->GetBufferSize(),nullptr,&g_ps)))return false;
    D3D11_INPUT_ELEMENT_DESC il[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(g_dev->CreateInputLayout(il,3,bv->GetBufferPointer(),bv->GetBufferSize(),&g_layout)))return false; rel(bv); rel(bp);
    D3D11_BUFFER_DESC cbd{}; cbd.ByteWidth=sizeof(CB); cbd.Usage=D3D11_USAGE_DEFAULT; cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER; if(FAILED(g_dev->CreateBuffer(&cbd,nullptr,&g_cb)))return false;
    D3D11_SAMPLER_DESC ss{}; ss.Filter=D3D11_FILTER_ANISOTROPIC; ss.AddressU=ss.AddressV=ss.AddressW=D3D11_TEXTURE_ADDRESS_WRAP; ss.MaxAnisotropy=8; ss.MaxLOD=D3D11_FLOAT32_MAX; if(FAILED(g_dev->CreateSamplerState(&ss,&g_sampler)))return false;
    std::wstring gp=findGroundTexture(); if(gp.empty()||!loadTextureWIC(gp,&g_groundSRV))return false; if(!loadTextureWIC(L"ExternalAssets\\KayKitDungeon\\dungeon_texture.png",&g_kaySRV))return false;
    return true;
}

static bool initUI(){
    const char* vs="struct I{float2 p:POSITION;float2 uv:TEXCOORD0;float4 c:COLOR;};struct O{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float4 c:COLOR;};O main(I i){O o;o.p=float4(i.p,0,1);o.uv=i.uv;o.c=i.c;return o;}";
    const char* ps="Texture2D t:register(t0);SamplerState s:register(s0);struct I{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float4 c:COLOR;};float4 main(I i):SV_TARGET{return t.Sample(s,i.uv)*i.c;}";
    ID3DBlob* a=nullptr,*b=nullptr; if(!compileShader(vs,"main","vs_5_0",&a)||!compileShader(ps,"main","ps_5_0",&b))return false;
    if(FAILED(g_dev->CreateVertexShader(a->GetBufferPointer(),a->GetBufferSize(),nullptr,&g_uiVS))||FAILED(g_dev->CreatePixelShader(b->GetBufferPointer(),b->GetBufferSize(),nullptr,&g_uiPS)))return false;
    D3D11_INPUT_ELEMENT_DESC il[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(g_dev->CreateInputLayout(il,3,a->GetBufferPointer(),a->GetBufferSize(),&g_uiLayout)))return false; rel(a); rel(b);
    D3D11_BUFFER_DESC bd{}; bd.ByteWidth=sizeof(UIVertex)*6; bd.Usage=D3D11_USAGE_DYNAMIC; bd.BindFlags=D3D11_BIND_VERTEX_BUFFER; bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE; if(FAILED(g_dev->CreateBuffer(&bd,nullptr,&g_uiVB)))return false;
    D3D11_BLEND_DESC bl{}; bl.RenderTarget[0].BlendEnable=TRUE; bl.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA; bl.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA; bl.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD; bl.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE; bl.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA; bl.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD; bl.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL; if(FAILED(g_dev->CreateBlendState(&bl,&g_uiBlend)))return false;
    D3D11_DEPTH_STENCIL_DESC ds{}; ds.DepthEnable=FALSE; ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO; ds.DepthFunc=D3D11_COMPARISON_ALWAYS; if(FAILED(g_dev->CreateDepthStencilState(&ds,&g_uiDepthOff)))return false;
    if(!loadTextureWIC(L"ExternalAssets\\KenneyUI\\panel_blue.png",&g_uiPanel))return false;
    if(!loadTextureWIC(L"ExternalAssets\\KenneyUI\\bar_green.png",&g_uiHealth))return false;
    if(!loadTextureWIC(L"ExternalAssets\\KenneyUI\\slot_yellow.png",&g_uiSlot))return false;
    return makeWhiteTexture();
}

static bool makeMesh(const std::vector<Vertex>& v,const std::vector<uint32_t>& idx,MeshGPU& m,bool dynamic=false){
    if(v.empty()||idx.empty())return false; m.count=(UINT)idx.size(); m.bmin=m.bmax=v[0].p;
    for(const auto& a:v){ m.bmin.x=(std::min)(m.bmin.x,a.p.x);m.bmin.y=(std::min)(m.bmin.y,a.p.y);m.bmin.z=(std::min)(m.bmin.z,a.p.z);m.bmax.x=(std::max)(m.bmax.x,a.p.x);m.bmax.y=(std::max)(m.bmax.y,a.p.y);m.bmax.z=(std::max)(m.bmax.z,a.p.z); }
    D3D11_BUFFER_DESC bd{}; bd.ByteWidth=(UINT)(v.size()*sizeof(Vertex)); bd.Usage=dynamic?D3D11_USAGE_DYNAMIC:D3D11_USAGE_DEFAULT; bd.BindFlags=D3D11_BIND_VERTEX_BUFFER; bd.CPUAccessFlags=dynamic?D3D11_CPU_ACCESS_WRITE:0; D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem=v.data();
    if(dynamic){ if(FAILED(g_dev->CreateBuffer(&bd,nullptr,&m.vb)))return false; D3D11_MAPPED_SUBRESOURCE ms{}; if(FAILED(g_ctx->Map(m.vb,0,D3D11_MAP_WRITE_DISCARD,0,&ms)))return false; memcpy(ms.pData,v.data(),v.size()*sizeof(Vertex)); g_ctx->Unmap(m.vb,0); }
    else if(FAILED(g_dev->CreateBuffer(&bd,&sd,&m.vb)))return false;
    bd={}; bd.ByteWidth=(UINT)(idx.size()*sizeof(uint32_t)); bd.Usage=D3D11_USAGE_DEFAULT; bd.BindFlags=D3D11_BIND_INDEX_BUFFER; sd={}; sd.pSysMem=idx.data(); return SUCCEEDED(g_dev->CreateBuffer(&bd,&sd,&m.ib));
}
static void killMesh(MeshGPU& m){ rel(m.vb); rel(m.ib); m.count=0; }

static bool loadOBJ(const std::wstring& path,MeshGPU& m){
    std::ifstream f(path); if(!f)return false; std::vector<XMFLOAT3> p,n; std::vector<XMFLOAT2> uv; std::vector<Vertex> v; std::vector<uint32_t> ind; std::unordered_map<std::string,uint32_t> map; std::string line;
    auto emit=[&](const std::string& s)->uint32_t{ auto it=map.find(s); if(it!=map.end())return it->second; int pi=0,ti=0,ni=0; sscanf_s(s.c_str(),"%d/%d/%d",&pi,&ti,&ni); Vertex a{}; if(pi>0&&pi<=(int)p.size())a.p=p[pi-1]; if(ni>0&&ni<=(int)n.size())a.n=n[ni-1]; else a.n={0,1,0}; if(ti>0&&ti<=(int)uv.size())a.uv=uv[ti-1]; uint32_t id=(uint32_t)v.size(); v.push_back(a); map[s]=id; return id; };
    while(std::getline(f,line)){ std::istringstream ss(line); std::string t; ss>>t; if(t=="v"){XMFLOAT3 a;ss>>a.x>>a.y>>a.z;p.push_back(a);} else if(t=="vn"){XMFLOAT3 a;ss>>a.x>>a.y>>a.z;n.push_back(a);} else if(t=="vt"){XMFLOAT2 a;ss>>a.x>>a.y;a.y=1.f-a.y;uv.push_back(a);} else if(t=="f"){std::vector<std::string> face;std::string q;while(ss>>q)face.push_back(q);for(size_t k=1;k+1<face.size();++k){ind.push_back(emit(face[0]));ind.push_back(emit(face[k]));ind.push_back(emit(face[k+1]));}} }
    return makeMesh(v,ind,m,false);
}

static float hashf(float x,float z){ float v=sinf(x*12.9898f+z*78.233f)*43758.5453f; return v-floorf(v); }
static float groundH(float x,float z){ float h=sinf(x*.035f)*1.2f+cosf(z*.031f)*.9f+sinf((x+z)*.075f)*.35f+(hashf(floorf(x*.15f),floorf(z*.15f))-.5f)*.08f; return h*.28f; }
static XMFLOAT3 terrainNormal(float x,float z){ float e=.2f,dx=groundH(x+e,z)-groundH(x-e,z),dz=groundH(x,z+e)-groundH(x,z-e); XMVECTOR n=XMVector3Normalize(XMVectorSet(-dx,2*e,-dz,0)); XMFLOAT3 o;XMStoreFloat3(&o,n);return o; }
static void buildTerrain(){
    const int N=181; const float SIZE=220.f,step=SIZE/(N-1),half=SIZE*.5f; std::vector<Vertex> v; std::vector<uint32_t> i; v.reserve(N*N);
    for(int z=0;z<N;++z)for(int x=0;x<N;++x){ float wx=-half+x*step,wz=-half+z*step; v.push_back({{wx,groundH(wx,wz),wz},terrainNormal(wx,wz),{wx*.055f,wz*.055f}}); }
    for(int z=0;z<N-1;++z)for(int x=0;x<N-1;++x){ uint32_t a=z*N+x; i.insert(i.end(),{a,a+N,a+1,a+1,a+N,a+N+1}); }
    makeMesh(v,i,g_terrain,false);
}
static XMMATRIX objectWorld(const SceneObj& o){ return XMMatrixScaling(o.scale,o.scale,o.scale)*XMMatrixRotationY(o.yaw)*XMMatrixTranslation(o.x,groundH(o.x,o.z),o.z); }

struct SkinVertex { XMFLOAT3 p,n; XMFLOAT2 uv; uint16_t joint[4]{}; float weight[4]{}; };
struct SkinnedMesh { std::vector<SkinVertex> source; std::vector<Vertex> posed; MeshGPU gpu; };
struct Bone { std::string name; int parent=-1; XMFLOAT3 t{0,0,0},s{1,1,1}; XMFLOAT4 r{0,0,0,1}; XMMATRIX invBind=XMMatrixIdentity(); };
struct Character {
    std::vector<SkinnedMesh> meshes; std::vector<Bone> bones; std::unordered_map<std::string,int> boneByName; ID3D11ShaderResourceView* texture=nullptr;
    XMFLOAT3 pos{0,0,0},home{0,0,0}; float yaw=0,animTime=0,speed=0,aiPhase=0; bool moving=false; float health=1.f;
};
static Character g_player,g_npc;

struct AnimTrack { enum Path{Translation,Rotation,Scale} path=Translation; std::string bone; std::vector<float> time; std::vector<XMFLOAT4> value; };
struct AnimClip { std::string name; float duration=0; std::vector<AnimTrack> tracks; };
static std::unordered_map<std::string,AnimClip> g_clips;

static XMMATRIX gltfMatrixRow(const float* m){ return XMMATRIX(m[0],m[4],m[8],m[12], m[1],m[5],m[9],m[13], m[2],m[6],m[10],m[14], m[3],m[7],m[11],m[15]); }
static XMMATRIX trsMatrix(const XMFLOAT3& t,const XMFLOAT4& r,const XMFLOAT3& s){ return XMMatrixScaling(s.x,s.y,s.z)*XMMatrixRotationQuaternion(XMLoadFloat4(&r))*XMMatrixTranslation(t.x,t.y,t.z); }

static bool loadCharacter(const char* path,Character& out){
    cgltf_options opt{}; cgltf_data* data=nullptr; if(cgltf_parse_file(&opt,path,&data)!=cgltf_result_success)return false; if(cgltf_load_buffers(&opt,data,path)!=cgltf_result_success){cgltf_free(data);return false;}
    if(data->skins_count<1){cgltf_free(data);return false;} cgltf_skin* skin=&data->skins[0]; out.bones.resize(skin->joints_count);
    std::unordered_map<const cgltf_node*,int> nodeBone;
    for(cgltf_size i=0;i<skin->joints_count;++i){ cgltf_node* n=skin->joints[i]; Bone& b=out.bones[i]; b.name=n->name?n->name:""; if(n->has_translation)b.t={n->translation[0],n->translation[1],n->translation[2]}; if(n->has_rotation)b.r={n->rotation[0],n->rotation[1],n->rotation[2],n->rotation[3]}; if(n->has_scale)b.s={n->scale[0],n->scale[1],n->scale[2]}; out.boneByName[b.name]=(int)i; nodeBone[n]=(int)i; }
    for(cgltf_size i=0;i<skin->joints_count;++i){ cgltf_node* p=skin->joints[i]->parent; auto it=nodeBone.find(p); if(it!=nodeBone.end())out.bones[i].parent=it->second; }
    if(skin->inverse_bind_matrices){ for(cgltf_size i=0;i<skin->joints_count;++i){ float m[16]{}; if(cgltf_accessor_read_float(skin->inverse_bind_matrices,i,m,16))out.bones[i].invBind=gltfMatrixRow(m); } }
    bool any=false;
    for(cgltf_size ni=0;ni<data->nodes_count;++ni){ cgltf_node* node=&data->nodes[ni]; if(!node->mesh||node->skin!=skin)continue;
        for(cgltf_size pi=0;pi<node->mesh->primitives_count;++pi){ cgltf_primitive* pr=&node->mesh->primitives[pi]; const cgltf_accessor *pa=nullptr,*na=nullptr,*ua=nullptr,*ja=nullptr,*wa=nullptr;
            for(cgltf_size ai=0;ai<pr->attributes_count;++ai){ cgltf_attribute& a=pr->attributes[ai]; if(a.type==cgltf_attribute_type_position)pa=a.data; else if(a.type==cgltf_attribute_type_normal)na=a.data; else if(a.type==cgltf_attribute_type_texcoord&&a.index==0)ua=a.data; else if(a.type==cgltf_attribute_type_joints&&a.index==0)ja=a.data; else if(a.type==cgltf_attribute_type_weights&&a.index==0)wa=a.data; }
            if(!pa||!ja||!wa)continue; SkinnedMesh sm; sm.source.resize(pa->count); sm.posed.resize(pa->count);
            for(cgltf_size k=0;k<pa->count;++k){ float p[3]{},n[3]{0,1,0},uv[2]{},w[4]{}; cgltf_uint j[4]{}; cgltf_accessor_read_float(pa,k,p,3); if(na)cgltf_accessor_read_float(na,k,n,3); if(ua)cgltf_accessor_read_float(ua,k,uv,2); cgltf_accessor_read_uint(ja,k,j,4); cgltf_accessor_read_float(wa,k,w,4); SkinVertex& sv=sm.source[k]; sv.p={p[0],p[1],p[2]};sv.n={n[0],n[1],n[2]};sv.uv={uv[0],1.f-uv[1]}; for(int q=0;q<4;++q){sv.joint[q]=(uint16_t)j[q];sv.weight[q]=w[q];} sm.posed[k]={sv.p,sv.n,sv.uv}; }
            std::vector<uint32_t> idx; if(pr->indices){idx.resize(pr->indices->count);for(cgltf_size k=0;k<pr->indices->count;++k)idx[k]=(uint32_t)cgltf_accessor_read_index(pr->indices,k);} else{idx.resize(sm.posed.size());for(uint32_t k=0;k<(uint32_t)idx.size();++k)idx[k]=k;}
            if(makeMesh(sm.posed,idx,sm.gpu,true)){out.meshes.push_back(std::move(sm));any=true;}
        }
    }
    cgltf_free(data); return any;
}

static void killCharacter(Character& c){ for(auto& m:c.meshes)killMesh(m.gpu); c.meshes.clear(); rel(c.texture); }

static bool loadAnimationClips(const char* path,const std::vector<std::string>& wanted){
    cgltf_options opt{}; cgltf_data* data=nullptr; if(cgltf_parse_file(&opt,path,&data)!=cgltf_result_success)return false; if(cgltf_load_buffers(&opt,data,path)!=cgltf_result_success){cgltf_free(data);return false;} bool any=false;
    for(cgltf_size ai=0;ai<data->animations_count;++ai){ cgltf_animation& a=data->animations[ai]; std::string name=a.name?a.name:""; if(std::find(wanted.begin(),wanted.end(),name)==wanted.end())continue; AnimClip clip;clip.name=name;
        for(cgltf_size ci=0;ci<a.channels_count;++ci){ cgltf_animation_channel& ch=a.channels[ci]; if(!ch.target_node||!ch.target_node->name||!ch.sampler||!ch.sampler->input||!ch.sampler->output)continue; AnimTrack tr;tr.bone=ch.target_node->name;
            if(ch.target_path==cgltf_animation_path_type_translation)tr.path=AnimTrack::Translation; else if(ch.target_path==cgltf_animation_path_type_rotation)tr.path=AnimTrack::Rotation; else if(ch.target_path==cgltf_animation_path_type_scale)tr.path=AnimTrack::Scale; else continue;
            cgltf_accessor* ta=ch.sampler->input; cgltf_accessor* va=ch.sampler->output; tr.time.resize(ta->count); tr.value.resize(ta->count);
            for(cgltf_size k=0;k<ta->count;++k){ float tt=0;cgltf_accessor_read_float(ta,k,&tt,1);tr.time[k]=tt;clip.duration=(std::max)(clip.duration,tt); if(tr.path==AnimTrack::Rotation){float q[4]{};cgltf_accessor_read_float(va,k,q,4);tr.value[k]={q[0],q[1],q[2],q[3]};}else{float q[3]{};cgltf_accessor_read_float(va,k,q,3);tr.value[k]={q[0],q[1],q[2],0};} }
            clip.tracks.push_back(std::move(tr));
        }
        g_clips[name]=std::move(clip); any=true;
    }
    cgltf_free(data); return any;
}

static XMFLOAT4 sampleTrack(const AnimTrack& tr,float t){
    if(tr.time.empty())return {0,0,0,tr.path==AnimTrack::Rotation?1.f:0.f}; if(tr.time.size()==1||t<=tr.time.front())return tr.value.front(); if(t>=tr.time.back())return tr.value.back(); auto it=std::upper_bound(tr.time.begin(),tr.time.end(),t); size_t b=(size_t)(it-tr.time.begin()),a=b-1; float u=(t-tr.time[a])/(tr.time[b]-tr.time[a]);
    if(tr.path==AnimTrack::Rotation){ XMVECTOR qa=XMLoadFloat4(&tr.value[a]),qb=XMLoadFloat4(&tr.value[b]); if(XMVectorGetX(XMVector4Dot(qa,qb))<0)qb=XMVectorNegate(qb); XMFLOAT4 r;XMStoreFloat4(&r,XMQuaternionNormalize(XMQuaternionSlerp(qa,qb,u)));return r; }
    XMFLOAT4 r; r.x=tr.value[a].x+(tr.value[b].x-tr.value[a].x)*u; r.y=tr.value[a].y+(tr.value[b].y-tr.value[a].y)*u; r.z=tr.value[a].z+(tr.value[b].z-tr.value[a].z)*u; r.w=0; return r;
}

static void poseCharacter(Character& c,const char* clipName,float dt,float rate=1.f){
    auto itc=g_clips.find(clipName); if(itc==g_clips.end()||c.bones.empty())return; const AnimClip& clip=itc->second; c.animTime+=dt*rate; float t=clip.duration>0?fmodf(c.animTime,clip.duration):0;
    std::vector<XMFLOAT3> T(c.bones.size()),S(c.bones.size()); std::vector<XMFLOAT4> R(c.bones.size());
    for(size_t i=0;i<c.bones.size();++i){T[i]=c.bones[i].t;S[i]=c.bones[i].s;R[i]=c.bones[i].r;}
    for(const auto& tr:clip.tracks){ auto ib=c.boneByName.find(tr.bone); if(ib==c.boneByName.end())continue; int b=ib->second; XMFLOAT4 v=sampleTrack(tr,t); if(tr.path==AnimTrack::Translation)T[b]={v.x,v.y,v.z}; else if(tr.path==AnimTrack::Scale)S[b]={v.x,v.y,v.z}; else R[b]=v; }
    std::vector<XMMATRIX> local(c.bones.size()),world(c.bones.size()),skin(c.bones.size()); std::vector<uint8_t> done(c.bones.size(),0);
    for(size_t i=0;i<c.bones.size();++i)local[i]=trsMatrix(T[i],R[i],S[i]);
    auto calc=[&](auto&& self,int i)->XMMATRIX{ if(done[i])return world[i]; int p=c.bones[i].parent; world[i]=(p>=0)?local[i]*self(self,p):local[i]; done[i]=1; return world[i]; };
    for(size_t i=0;i<c.bones.size();++i)skin[i]=c.bones[i].invBind*calc(calc,(int)i);
    for(auto& m:c.meshes){
        for(size_t vi=0;vi<m.source.size();++vi){ const SkinVertex& s=m.source[vi]; XMVECTOR ps=XMVectorZero(),ns=XMVectorZero(); float tw=0;
            for(int q=0;q<4;++q){ float w=s.weight[q]; uint16_t j=s.joint[q]; if(w<=0||j>=skin.size())continue; XMVECTOR p=XMVector3TransformCoord(XMLoadFloat3(&s.p),skin[j]); XMVECTOR n=XMVector3TransformNormal(XMLoadFloat3(&s.n),skin[j]); ps=XMVectorAdd(ps,XMVectorScale(p,w)); ns=XMVectorAdd(ns,XMVectorScale(n,w)); tw+=w; }
            if(tw<.001f){ps=XMLoadFloat3(&s.p);ns=XMLoadFloat3(&s.n);} XMFLOAT3 p,n;XMStoreFloat3(&p,ps);XMStoreFloat3(&n,XMVector3Normalize(ns));m.posed[vi]={p,n,s.uv};
        }
        D3D11_MAPPED_SUBRESOURCE ms{}; if(SUCCEEDED(g_ctx->Map(m.gpu.vb,0,D3D11_MAP_WRITE_DISCARD,0,&ms))){memcpy(ms.pData,m.posed.data(),m.posed.size()*sizeof(Vertex));g_ctx->Unmap(m.gpu.vb,0);}
    }
}

static void resolveCharacter(Character& c){
    const float cr=.42f; for(const auto& o:g_scene){ if(!o.collider||!o.mesh)continue; float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.42f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.42f,r=(std::max)(.55f,(std::max)(rx,rz)); float dx=c.pos.x-o.x,dz=c.pos.z-o.z,ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr); if(ds<rr&&ds>1e-6f){float d=sqrtf(ds),push=r+cr-d;c.pos.x+=dx/d*push;c.pos.z+=dz/d*push;} } c.pos.y=groundH(c.pos.x,c.pos.z);
}

static void updatePlayer(float dt){
    float side=(g_keys['D']?1.f:0.f)-(g_keys['A']?1.f:0.f),fwd=(g_keys['W']?1.f:0.f)-(g_keys['S']?1.f:0.f); float m=sqrtf(side*side+fwd*fwd); if(m>1){side/=m;fwd/=m;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd; float dm=sqrtf(dx*dx+dz*dz); g_player.moving=dm>.01f; g_player.speed=g_player.moving?4.8f:0;
    if(g_player.moving){dx/=dm;dz/=dm;g_player.yaw=approachAngle(g_player.yaw,atan2f(dx,dz),10.f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;}
    resolveCharacter(g_player); poseCharacter(g_player,g_player.moving?"Running_A":"Idle_A",dt,g_player.moving?1.05f:1.f);
}

static void updateNPC(float dt){
    g_npc.aiPhase+=dt*.28f; XMFLOAT3 target={g_npc.home.x+cosf(g_npc.aiPhase)*7.5f,0,g_npc.home.z+sinf(g_npc.aiPhase)*5.5f}; float dx=target.x-g_npc.pos.x,dz=target.z-g_npc.pos.z,d=sqrtf(dx*dx+dz*dz); g_npc.moving=d>.15f; g_npc.speed=g_npc.moving?2.4f:0;
    if(g_npc.moving){dx/=d;dz/=d;g_npc.yaw=approachAngle(g_npc.yaw,atan2f(dx,dz),6.f*dt);g_npc.pos.x+=dx*g_npc.speed*dt;g_npc.pos.z+=dz*g_npc.speed*dt;}
    resolveCharacter(g_npc); poseCharacter(g_npc,g_npc.moving?"Walking_A":"Idle_A",dt,1.f);
}

static void updateCamera(float dt){
    XMFLOAT3 want={g_player.pos.x,g_player.pos.y+1.15f,g_player.pos.z}; float k=1.f-expf(-8.f*dt); g_camTarget.x+=(want.x-g_camTarget.x)*k;g_camTarget.y+=(want.y-g_camTarget.y)*k;g_camTarget.z+=(want.z-g_camTarget.z)*k;
    float dist=12.5f*g_zoom,h=dist*sinf(g_camPitch),flat=dist*cosf(g_camPitch); g_camEye={g_camTarget.x-sinf(g_camYaw)*flat,g_camTarget.y+h,g_camTarget.z-cosf(g_camYaw)*flat};
}

static void drawMeshWorld(const MeshGPU& m,const XMMATRIX& world,const XMFLOAT4& color,float useTex,ID3D11ShaderResourceView* srv){
    if(!m.vb||!m.ib||!m.count)return; UINT st=sizeof(Vertex),off=0; g_ctx->IASetVertexBuffers(0,1,&m.vb,&st,&off); g_ctx->IASetIndexBuffer(m.ib,DXGI_FORMAT_R32_UINT,0); g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    XMVECTOR eye=XMLoadFloat3(&g_camEye),at=XMLoadFloat3(&g_camTarget),up=XMVectorSet(0,1,0,0); XMMATRIX view=XMMatrixLookAtLH(eye,at,up),proj=XMMatrixPerspectiveFovLH(XMConvertToRadians(48.f),g_w/(float)(std::max)(1,g_h),.1f,400.f);
    CB cb{}; cb.world=XMMatrixTranspose(world);cb.view=XMMatrixTranspose(view);cb.proj=XMMatrixTranspose(proj);cb.color=color;cb.lightDir={-.35f,-1.f,.28f};cb.useTex=useTex;cb.cameraPos=g_camEye;cb.fogStart=70.f;cb.fogColor={.48f,.62f,.70f};cb.fogEnd=125.f;
    g_ctx->UpdateSubresource(g_cb,0,nullptr,&cb,0,0);g_ctx->VSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetShaderResources(0,1,&srv);g_ctx->PSSetSamplers(0,1,&g_sampler);g_ctx->DrawIndexed(m.count,0,0);
}

static void drawCharacter(const Character& c){ XMMATRIX actor=XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y,c.pos.z); for(const auto& m:c.meshes)drawMeshWorld(m.gpu,actor,{1,1,1,1},1,c.texture); }

static void drawUIQuad(ID3D11ShaderResourceView* tex,float x,float y,float w,float h,const XMFLOAT4& col){
    float l=2*x/g_w-1,r=2*(x+w)/g_w-1,t=1-2*y/g_h,b=1-2*(y+h)/g_h; UIVertex v[6]={{{l,t},{0,0},col},{{r,t},{1,0},col},{{r,b},{1,1},col},{{l,t},{0,0},col},{{r,b},{1,1},col},{{l,b},{0,1},col}};
    D3D11_MAPPED_SUBRESOURCE ms{}; if(FAILED(g_ctx->Map(g_uiVB,0,D3D11_MAP_WRITE_DISCARD,0,&ms)))return; memcpy(ms.pData,v,sizeof(v));g_ctx->Unmap(g_uiVB,0);UINT st=sizeof(UIVertex),off=0;g_ctx->IASetInputLayout(g_uiLayout);g_ctx->IASetVertexBuffers(0,1,&g_uiVB,&st,&off);g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);g_ctx->VSSetShader(g_uiVS,nullptr,0);g_ctx->PSSetShader(g_uiPS,nullptr,0);g_ctx->PSSetShaderResources(0,1,&tex);g_ctx->PSSetSamplers(0,1,&g_sampler);float f[4]{};g_ctx->OMSetBlendState(g_uiBlend,f,0xffffffff);g_ctx->OMSetDepthStencilState(g_uiDepthOff,0);g_ctx->Draw(6,0);
}
static void drawHUD(){
    float cx=g_w*.5f; drawUIQuad(g_uiPanel,cx-270.f,g_h-116.f,540.f,92.f,{1,1,1,.96f}); drawUIQuad(g_uiHealth,cx-240.f,g_h-94.f,210.f*g_player.health,28.f,{1,1,1,1});
    for(int i=0;i<5;++i)drawUIQuad(i==0?g_uiSlot:g_uiPanel,cx+4.f+i*52.f,g_h-96.f,46.f,46.f,{1,1,1,1});
    drawUIQuad(g_uiWhite,cx-7.f,g_h*.5f-1.f,14.f,2.f,{1,1,1,.75f}); drawUIQuad(g_uiWhite,cx-1.f,g_h*.5f-7.f,2.f,14.f,{1,1,1,.75f});
}

static void render(){
    if(!g_rtv)return; float clear[]={.48f,.62f,.70f,1}; g_ctx->OMSetRenderTargets(1,&g_rtv,g_dsv);g_ctx->ClearRenderTargetView(g_rtv,clear);g_ctx->ClearDepthStencilView(g_dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);D3D11_VIEWPORT vp={0,0,(float)g_w,(float)g_h,0,1};g_ctx->RSSetViewports(1,&vp);g_ctx->IASetInputLayout(g_layout);g_ctx->VSSetShader(g_vs,nullptr,0);g_ctx->PSSetShader(g_ps,nullptr,0);g_ctx->OMSetDepthStencilState(nullptr,0);
    drawMeshWorld(g_terrain,XMMatrixIdentity(),{.92f,.92f,.92f,1},1,g_groundSRV); for(const auto& o:g_scene)drawMeshWorld(*o.mesh,objectWorld(o),{1,1,1,1},1,g_kaySRV); drawCharacter(g_npc);drawCharacter(g_player);drawHUD();g_swap->Present(1,0);
}

static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_NCHITTEST:{LRESULT r=DefWindowProcW(h,m,w,l);return r==HTCLIENT&&GET_Y_LPARAM(l)-[] (HWND q){RECT r;GetWindowRect(q,&r);return r.top;}(h)<6?HTCAPTION:r;}
        case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;
        case WM_LBUTTONDOWN:g_lmb=true;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
        case WM_LBUTTONUP:g_lmb=false;ReleaseCapture();return 0;
        case WM_MOUSEMOVE:if(g_lmb){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int dx=p.x-g_lastMouse.x,dy=p.y-g_lastMouse.y;g_camYaw+=dx*.0065f;g_camPitch=clampf(g_camPitch+dy*.0045f,.28f,1.08f);g_lastMouse=p;}return 0;
        case WM_MOUSEWHEEL:g_zoom*=GET_WHEEL_DELTA_WPARAM(w)>0?.90f:1.11f;g_zoom=clampf(g_zoom,.60f,1.65f);return 0;
        case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w==VK_ESCAPE)DestroyWindow(h);return 0;
        case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;
        case WM_KILLFOCUS:for(bool& k:g_keys)k=false;g_lmb=false;ReleaseCapture();return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); WNDCLASSW wc{};wc.hInstance=hi;wc.lpfnWndProc=wnd;wc.lpszClassName=L"SP_HUMAN_DEMO";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);RECT wa{};SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);int aw=wa.right-wa.left,ah=wa.bottom-wa.top;g_w=(std::min)(1440,(std::max)(960,aw-80));g_h=(std::min)(900,(std::max)(680,ah-80));g_w=(std::min)(g_w,aw);g_h=(std::min)(g_h,ah);int x=wa.left+(aw-g_w)/2,y=wa.top+(ah-g_h)/2;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;
    if(!initD3D()||!initUI()){MessageBoxW(g_hwnd,L"No pude iniciar Direct3D o el HUD.",L"SPVersion",MB_ICONERROR);return 2;} buildTerrain(); bool scene=loadOBJ(L"ExternalAssets\\KayKitDungeon\\rubble_large.obj",g_rubble)&&loadOBJ(L"ExternalAssets\\KayKitDungeon\\crates_stacked.obj",g_crates)&&loadOBJ(L"ExternalAssets\\KayKitDungeon\\barrel_large.obj",g_barrel); if(!scene){MessageBoxW(g_hwnd,L"Faltan assets KayKit Dungeon.",L"SPVersion",MB_ICONERROR);return 3;}
    auto add=[&](MeshGPU& m,float px,float pz,float yaw,float sc){g_scene.push_back({&m,px,pz,yaw,sc,true});}; for(int i=-4;i<=4;++i){add((i%3==0)?g_crates:((i&1)?g_barrel:g_rubble),i*16.f,24.f+(i&1)*12.f,i*.31f,1.25f);} for(int i=-4;i<=4;++i)add((i&1)?g_rubble:g_crates,i*18.f,-36.f-(i&1)*9.f,-i*.27f,1.2f);
    if(!loadCharacter("ExternalAssets/KayKitAdventurers/Knight.glb",g_player)||!loadCharacter("ExternalAssets/KayKitAdventurers/Ranger.glb",g_npc)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\knight_texture.png",&g_player.texture)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\ranger_texture.png",&g_npc.texture)){MessageBoxW(g_hwnd,L"No pude cargar Knight/Ranger.",L"SPVersion",MB_ICONERROR);return 4;}
    if(!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_General.glb",{"Idle_A"})||!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_MovementBasic.glb",{"Walking_A","Running_A"})){MessageBoxW(g_hwnd,L"No pude cargar las animaciones KayKit.",L"SPVersion",MB_ICONERROR);return 5;}
    g_player.pos={0,groundH(0,0),0};g_player.home=g_player.pos;g_npc.pos={8,groundH(8,9),9};g_npc.home=g_npc.pos;g_camTarget={0,1,0};poseCharacter(g_player,"Idle_A",0);poseCharacter(g_npc,"Walking_A",0);
    LARGE_INTEGER fq,p,n;QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&p);MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}QueryPerformanceCounter(&n);float dt=(float)((double)(n.QuadPart-p.QuadPart)/fq.QuadPart);p=n;dt=(std::min)(dt,.033f);syncClientSize();updatePlayer(dt);updateNPC(dt);updateCamera(dt);render();Sleep(1);} 
    killCharacter(g_npc);killCharacter(g_player);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);rel(g_uiWhite);rel(g_uiSlot);rel(g_uiHealth);rel(g_uiPanel);rel(g_uiDepthOff);rel(g_uiBlend);rel(g_uiVB);rel(g_uiLayout);rel(g_uiPS);rel(g_uiVS);rel(g_kaySRV);rel(g_groundSRV);rel(g_sampler);rel(g_cb);rel(g_layout);rel(g_ps);rel(g_vs);destroySize();rel(g_swap);rel(g_ctx);rel(g_dev);CoUninitialize();return 0;
}
