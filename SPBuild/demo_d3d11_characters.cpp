#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define wWinMain legacy_blob_entry
#define wnd legacy_blob_wnd
#include "demo_d3d11_nonnewtonian.cpp"
#undef wnd
#undef wWinMain

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <array>
#include <cstdio>

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

struct CharPart {
    MeshGPU mesh;
    enum Kind { Core, ArmL, ArmR, LegL, LegR } kind=Core;
};
struct Character {
    std::vector<CharPart> parts;
    ID3D11ShaderResourceView* texture=nullptr;
    XMFLOAT3 pos{0,0,0};
    float yaw=0.f;
    float phase=0.f;
    float speed=0.f;
    bool moving=false;
};

struct HudVertex{float x,y;float r,g,b,a;};
static ID3D11VertexShader* g_hudVS2=nullptr;
static ID3D11PixelShader* g_hudPS2=nullptr;
static ID3D11InputLayout* g_hudLayout2=nullptr;
static ID3D11Buffer* g_hudVB2=nullptr;
static ID3D11BlendState* g_hudBlend2=nullptr;
static Character g_player,g_npc;
static XMFLOAT3 g_camTarget{0,0,0};
static float g_camYaw=.72f,g_camPitch=.62f,g_camPanX=0.f,g_camPanZ=0.f;
static bool g_followCamera=true,g_rmb=false,g_mmb=false;
static POINT g_lastMouse{};

static float angleWrap(float a){while(a>XM_PI)a-=XM_2PI;while(a<-XM_PI)a+=XM_2PI;return a;}
static float approachAngle(float a,float b,float maxStep){float d=angleWrap(b-a);if(fabsf(d)<=maxStep)return b;return angleWrap(a+(d>0?maxStep:-maxStep));}

static CharPart::Kind classifyPart(const char* n){
    std::string s=n?n:"";
    if(s.find("ArmLeft")!=std::string::npos)return CharPart::ArmL;
    if(s.find("ArmRight")!=std::string::npos)return CharPart::ArmR;
    if(s.find("LegLeft")!=std::string::npos)return CharPart::LegL;
    if(s.find("LegRight")!=std::string::npos)return CharPart::LegR;
    return CharPart::Core;
}

static bool loadCharacterGLB(const char* path,Character& out){
    cgltf_options opt{};cgltf_data*data=nullptr;
    if(cgltf_parse_file(&opt,path,&data)!=cgltf_result_success)return false;
    if(cgltf_load_buffers(&opt,data,path)!=cgltf_result_success){cgltf_free(data);return false;}
    bool any=false;
    for(cgltf_size ni=0;ni<data->nodes_count;++ni){
        cgltf_node*node=&data->nodes[ni];if(!node->mesh)continue;
        for(cgltf_size pi=0;pi<node->mesh->primitives_count;++pi){
            cgltf_primitive*pr=&node->mesh->primitives[pi];
            const cgltf_accessor *pa=nullptr,*na=nullptr,*ua=nullptr;
            for(cgltf_size ai=0;ai<pr->attributes_count;++ai){
                cgltf_attribute&a=pr->attributes[ai];
                if(a.type==cgltf_attribute_type_position)pa=a.data;
                else if(a.type==cgltf_attribute_type_normal)na=a.data;
                else if(a.type==cgltf_attribute_type_texcoord&&a.index==0)ua=a.data;
            }
            if(!pa)continue;
            std::vector<Vertex>v(pa->count);std::vector<uint32_t>idx;
            for(cgltf_size k=0;k<pa->count;++k){
                float p[3]{},n[3]{0,1,0},uv[2]{};cgltf_accessor_read_float(pa,k,p,3);
                if(na)cgltf_accessor_read_float(na,k,n,3);if(ua)cgltf_accessor_read_float(ua,k,uv,2);
                v[k]={{p[0],p[1],p[2]},{n[0],n[1],n[2]},{uv[0],1.f-uv[1]}};
            }
            if(pr->indices){idx.resize(pr->indices->count);for(cgltf_size k=0;k<pr->indices->count;++k)idx[k]=(uint32_t)cgltf_accessor_read_index(pr->indices,k);}
            else{idx.resize(v.size());for(uint32_t k=0;k<(uint32_t)v.size();++k)idx[k]=k;}
            CharPart part{};part.kind=classifyPart(node->name?node->name:node->mesh->name);
            if(makeMesh(v,idx,part.mesh,false)){out.parts.push_back(part);any=true;}
        }
    }
    cgltf_free(data);return any;
}

static void killCharacter(Character&c){for(auto&p:c.parts)killMesh(p.mesh);rel(c.texture);c.parts.clear();}

static XMMATRIX localPartMatrix(CharPart::Kind kind,float phase,bool moving){
    float leg=moving?sinf(phase)*.53f:0.f;
    float arm=moving?sinf(phase)*.36f:0.f;
    XMFLOAT3 pivot{0,0,0};float rz=0,rx=0;
    switch(kind){
        case CharPart::ArmL:pivot={.13f,1.14f,0};rz=-XM_PIDIV2;rx=-arm;break;
        case CharPart::ArmR:pivot={-.13f,1.14f,0};rz=XM_PIDIV2;rx=arm;break;
        case CharPart::LegL:pivot={.17f,.52f,0};rx=leg;break;
        case CharPart::LegR:pivot={-.17f,.52f,0};rx=-leg;break;
        default:return XMMatrixIdentity();
    }
    return XMMatrixTranslation(-pivot.x,-pivot.y,-pivot.z)*XMMatrixRotationZ(rz)*XMMatrixRotationX(rx)*XMMatrixTranslation(pivot.x,pivot.y,pivot.z);
}

static void drawMeshWorld(const MeshGPU&m,const XMMATRIX&w,const XMFLOAT4&c,float useTex,ID3D11ShaderResourceView*srv){
    if(!m.vb||!m.ib||!m.count)return;UINT st=sizeof(Vertex),off=0;g_ctx->IASetVertexBuffers(0,1,&m.vb,&st,&off);g_ctx->IASetIndexBuffer(m.ib,DXGI_FORMAT_R32_UINT,0);g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    float orbit=31.f*g_zoom,horiz=orbit*cosf(g_camPitch),height=orbit*sinf(g_camPitch);
    XMVECTOR eye=XMVectorSet(g_camTarget.x-sinf(g_camYaw)*horiz,g_camTarget.y+height,g_camTarget.z-cosf(g_camYaw)*horiz,1),at=XMVectorSet(g_camTarget.x,g_camTarget.y,g_camTarget.z,1),up=XMVectorSet(0,1,0,0);
    XMMATRIX view=XMMatrixLookAtLH(eye,at,up),proj=XMMatrixPerspectiveFovLH(XMConvertToRadians(43.f),g_w/(float)(std::max)(1,g_h),.1f,650.f);
    CB cb{};cb.world=XMMatrixTranspose(w);cb.view=XMMatrixTranspose(view);cb.proj=XMMatrixTranspose(proj);cb.color=c;cb.lightDir={-.30f,-1.f,.22f};cb.useTex=useTex;
    g_ctx->UpdateSubresource(g_cb,0,nullptr,&cb,0,0);g_ctx->VSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetShaderResources(0,1,&srv);g_ctx->PSSetSamplers(0,1,&g_sampler);g_ctx->DrawIndexed(m.count,0,0);
}

static void drawCharacter(const Character&c){
    float bob=c.moving?(0.035f*(1.f-cosf(c.phase*2.f))):0.f;
    XMMATRIX actor=XMMatrixScaling(1.15f,1.15f,1.15f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y+bob,c.pos.z);
    for(const auto&p:c.parts){XMMATRIX w=localPartMatrix(p.kind,c.phase,c.moving)*actor;drawMeshWorld(p.mesh,w,{1,1,1,1},1,c.texture);}
}

static void resolveCharacter(Character&c){
    const float cr=.42f;
    for(const auto&o:g_scene){if(!o.collider||!o.mesh)continue;float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.48f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.48f;float r=(std::max)(.45f,(std::max)(rx,rz));float dx=c.pos.x-o.x,dz=c.pos.z-o.z,ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr);if(ds<rr&&ds>1e-6f){float d=sqrtf(ds),push=r+cr-d;c.pos.x+=dx/d*push;c.pos.z+=dz/d*push;}}
    c.pos.y=groundH(c.pos.x,c.pos.z);
}

static void updatePlayer(float dt){
    float side=0,fwd=0;if(g_keys['A'])side-=1;if(g_keys['D'])side+=1;if(g_keys['W'])fwd+=1;if(g_keys['S'])fwd-=1;
    float mag=sqrtf(side*side+fwd*fwd);if(mag>1.f){side/=mag;fwd/=mag;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd;float dm=sqrtf(dx*dx+dz*dz);
    g_player.moving=dm>.01f;g_player.speed=g_player.moving?4.2f:0.f;
    if(g_player.moving){dx/=dm;dz/=dm;float target=atan2f(dx,dz);g_player.yaw=approachAngle(g_player.yaw,target,8.5f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;g_player.phase+=dt*7.4f;}
    resolveCharacter(g_player);
    g_npc.moving=false;g_npc.phase+=dt*1.2f;g_npc.yaw=approachAngle(g_npc.yaw,atan2f(g_player.pos.x-g_npc.pos.x,g_player.pos.z-g_npc.pos.z),1.4f*dt);resolveCharacter(g_npc);
}

static void panCamera(float sx,float sy){float k=.030f*g_zoom;g_followCamera=false;g_camPanX+=(cosf(g_camYaw)*sx+sinf(g_camYaw)*sy)*k;g_camPanZ+=(-sinf(g_camYaw)*sx+cosf(g_camYaw)*sy)*k;}
static void updateCamera(float dt){
    POINT p{};GetCursorPos(&p);ScreenToClient(g_hwnd,&p);const int edge=14;float dx=0,dy=0;if(p.x>=0&&p.x<edge)dx-=1;if(p.x>=g_w-edge&&p.x<g_w)dx+=1;if(p.y>=0&&p.y<edge)dy+=1;if(p.y>=g_h-edge&&p.y<g_h)dy-=1;if(dx||dy)panCamera(dx*520.f*dt,dy*520.f*dt);
    XMFLOAT3 want=g_player.pos;if(!g_followCamera){want.x+=g_camPanX;want.z+=g_camPanZ;}else{g_camPanX=g_camPanZ=0;}
    float k=1.f-expf(-6.f*dt);g_camTarget.x+=(want.x-g_camTarget.x)*k;g_camTarget.z+=(want.z-g_camTarget.z)*k;g_camTarget.y=groundH(g_camTarget.x,g_camTarget.z)+1.1f;
}

static uint8_t glyphRow(char c,int r){c=(char)toupper((unsigned char)c);struct G{char c;std::array<uint8_t,7>a;};static const G g[]={
{'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},{'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},{'I',{31,4,4,4,4,4,31}},{'J',{7,2,2,2,18,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},{'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},{'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},{'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},{'.',{0,0,0,0,0,12,12}},{':',{0,12,12,0,12,12,0}},{'/',{1,2,4,8,16,0,0}},{' ',{0,0,0,0,0,0,0}}};for(auto&x:g)if(x.c==c)return x.a[r];return 0;}
static void hudRect(std::vector<HudVertex>&v,float x,float y,float w,float h,float r,float g,float b,float a){float l=2*x/g_w-1,rr=2*(x+w)/g_w-1,t=1-2*y/g_h,bb=1-2*(y+h)/g_h;HudVertex q[6]={{l,t,r,g,b,a},{rr,t,r,g,b,a},{rr,bb,r,g,b,a},{l,t,r,g,b,a},{rr,bb,r,g,b,a},{l,bb,r,g,b,a}};v.insert(v.end(),q,q+6);}
static void hudText(std::vector<HudVertex>&v,float x,float y,const char*s,float sc){float ox=x;for(;*s;++s){if(*s=='\n'){y+=9*sc;x=ox;continue;}for(int r=0;r<7;++r){uint8_t bits=glyphRow(*s,r);for(int c=0;c<5;++c)if(bits&(1<<(4-c)))hudRect(v,x+c*sc,y+r*sc,sc,sc,.94f,.97f,.92f,1);}x+=6*sc;}}
static bool initHud(){const char*vs="struct I{float2 p:POSITION;float4 c:COLOR;};struct O{float4 p:SV_POSITION;float4 c:COLOR;};O main(I i){O o;o.p=float4(i.p,0,1);o.c=i.c;return o;}";const char*ps="struct I{float4 p:SV_POSITION;float4 c:COLOR;};float4 main(I i):SV_TARGET{return i.c;}";ID3DBlob*a=nullptr,*b=nullptr;if(!compileShader(vs,"main","vs_5_0",&a)||!compileShader(ps,"main","ps_5_0",&b))return false;bool ok=SUCCEEDED(g_dev->CreateVertexShader(a->GetBufferPointer(),a->GetBufferSize(),nullptr,&g_hudVS2))&&SUCCEEDED(g_dev->CreatePixelShader(b->GetBufferPointer(),b->GetBufferSize(),nullptr,&g_hudPS2));D3D11_INPUT_ELEMENT_DESC il[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0}};ok=ok&&SUCCEEDED(g_dev->CreateInputLayout(il,2,a->GetBufferPointer(),a->GetBufferSize(),&g_hudLayout2));rel(a);rel(b);D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(HudVertex)*65536;bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;ok=ok&&SUCCEEDED(g_dev->CreateBuffer(&bd,nullptr,&g_hudVB2));D3D11_BLEND_DESC bl{};bl.RenderTarget[0].BlendEnable=TRUE;bl.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;bl.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;bl.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;bl.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;bl.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;bl.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;bl.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;ok=ok&&SUCCEEDED(g_dev->CreateBlendState(&bl,&g_hudBlend2));return ok;}
static void drawHud(){std::vector<HudVertex>v;v.reserve(12000);hudRect(v,14,14,650,68,.025f,.03f,.025f,.80f);char s[256];snprintf(s,sizeof(s),"WASD CAMINAR   MMB/BORDE CAMARA   RMB ROTAR   RUEDA ZOOM   F CENTRAR\nVELOCIDAD %.1f   CAM %s   ZOOM %.2f",g_player.speed,g_followCamera?"SIGUE":"LIBRE",g_zoom);hudText(v,27,26,s,1.55f);D3D11_MAPPED_SUBRESOURCE ms{};if(v.empty()||FAILED(g_ctx->Map(g_hudVB2,0,D3D11_MAP_WRITE_DISCARD,0,&ms)))return;memcpy(ms.pData,v.data(),v.size()*sizeof(HudVertex));g_ctx->Unmap(g_hudVB2,0);UINT st=sizeof(HudVertex),off=0;g_ctx->IASetInputLayout(g_hudLayout2);g_ctx->IASetVertexBuffers(0,1,&g_hudVB2,&st,&off);g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);g_ctx->VSSetShader(g_hudVS2,nullptr,0);g_ctx->PSSetShader(g_hudPS2,nullptr,0);float f[4]{};g_ctx->OMSetBlendState(g_hudBlend2,f,0xffffffff);g_ctx->OMSetDepthStencilState(nullptr,0);g_ctx->Draw((UINT)v.size(),0);g_ctx->OMSetBlendState(nullptr,f,0xffffffff);}

static void render(){if(!g_rtv)return;float clear[]={.26f,.34f,.38f,1};g_ctx->OMSetRenderTargets(1,&g_rtv,g_dsv);g_ctx->ClearRenderTargetView(g_rtv,clear);g_ctx->ClearDepthStencilView(g_dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);D3D11_VIEWPORT vp={0,0,(float)g_w,(float)g_h,0,1};g_ctx->RSSetViewports(1,&vp);g_ctx->IASetInputLayout(g_layout);g_ctx->VSSetShader(g_vs,nullptr,0);g_ctx->PSSetShader(g_ps,nullptr,0);drawMeshWorld(g_terrain,XMMatrixIdentity(),{1.08f,1.08f,1.08f,1},1,g_groundSRV);for(const auto&o:g_scene){XMMATRIX w=objectWorld(o);drawMeshWorld(*o.mesh,w,{1.12f,1.12f,1.12f,1},1,g_kaySRV);}drawCharacter(g_npc);drawCharacter(g_player);drawHud();g_swap->Present(1,0);}

static LRESULT CALLBACK charWnd(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);return p.y<7?HTCAPTION:HTCLIENT;}case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;case WM_MOUSEWHEEL:g_zoom*=GET_WHEEL_DELTA_WPARAM(w)>0?.90f:1.11f;g_zoom=clampf(g_zoom,.48f,2.8f);return 0;case WM_RBUTTONDOWN:g_rmb=true;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;case WM_MBUTTONDOWN:g_mmb=true;g_followCamera=false;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;case WM_MOUSEMOVE:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int dx=p.x-g_lastMouse.x,dy=p.y-g_lastMouse.y;if(g_rmb){g_camYaw+=dx*.0065f;g_camPitch=clampf(g_camPitch+dy*.0045f,.30f,1.12f);}if(g_mmb)panCamera((float)-dx,(float)dy);if(g_rmb||g_mmb)g_lastMouse=p;return 0;}case WM_RBUTTONUP:g_rmb=false;if(!g_mmb)ReleaseCapture();return 0;case WM_MBUTTONUP:g_mmb=false;if(!g_rmb)ReleaseCapture();return 0;case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='F'){g_followCamera=true;g_camPanX=g_camPanZ=0;}if(w==VK_ESCAPE)DestroyWindow(h);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(bool&k:g_keys)k=false;g_rmb=g_mmb=false;return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);WNDCLASSW wc{};wc.hInstance=hi;wc.lpfnWndProc=charWnd;wc.lpszClassName=L"SP_D3D11_CHARACTERS";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);RECT wa{};SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);int aw=wa.right-wa.left,ah=wa.bottom-wa.top;g_w=(std::min)(1440,(std::max)(900,aw-80));g_h=(std::min)(900,(std::max)(650,ah-80));g_w=(std::min)(g_w,aw);g_h=(std::min)(g_h,ah);int x=wa.left+(aw-g_w)/2,y=wa.top+(ah-g_h)/2;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;if(!initD3D()||!initHud()){MessageBoxW(g_hwnd,L"No pude iniciar Direct3D.",L"SPVersion",MB_ICONERROR);return 2;}buildTerrain();bool scene=true;scene&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\rubble_large.obj",g_rubble);scene&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\crates_stacked.obj",g_crates);scene&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\barrel_large.obj",g_barrel);if(!scene){MessageBoxW(g_hwnd,L"Faltan assets de escenario KayKit.",L"SPVersion",MB_ICONERROR);return 3;}g_scene.clear();auto add=[&](MeshGPU&m,float px,float pz,float yaw,float sc){g_scene.push_back({&m,px,pz,yaw,sc,true});};for(int i=-3;i<=3;++i){add((i%3==0)?g_crates:((i%3==1||i%3==-2)?g_barrel:g_rubble),i*15.f,18.f+(i&1)*12.f,i*.37f,1.35f);}for(int i=-3;i<=3;++i)add((i&1)?g_rubble:g_crates,i*17.f,-30.f-(i&1)*10.f,-i*.29f,1.3f);if(!loadCharacterGLB("ExternalAssets/KayKitAdventurers/Knight.glb",g_player)||!loadCharacterGLB("ExternalAssets/KayKitAdventurers/Ranger.glb",g_npc)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\knight_texture.png",&g_player.texture)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\ranger_texture.png",&g_npc.texture)){MessageBoxW(g_hwnd,L"No pude cargar Knight/Ranger de KayKit Adventurers.",L"SPVersion",MB_ICONERROR);return 4;}g_player.pos={0,groundH(0,0),0};g_player.yaw=0;g_npc.pos={7,groundH(7,8),8};g_npc.yaw=XM_PI;g_camTarget=g_player.pos;
LARGE_INTEGER fq,p,n;QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&p);MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}QueryPerformanceCounter(&n);float dt=(float)((double)(n.QuadPart-p.QuadPart)/fq.QuadPart);p=n;dt=(std::min)(dt,.033f);syncClientSize();updatePlayer(dt);updateCamera(dt);render();Sleep(1);}killCharacter(g_npc);killCharacter(g_player);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);rel(g_hudBlend2);rel(g_hudVB2);rel(g_hudLayout2);rel(g_hudPS2);rel(g_hudVS2);rel(g_kaySRV);rel(g_groundSRV);rel(g_sampler);rel(g_cb);rel(g_layout);rel(g_ps);rel(g_vs);destroySize();rel(g_swap);rel(g_ctx);rel(g_dev);CoUninitialize();return 0;}
