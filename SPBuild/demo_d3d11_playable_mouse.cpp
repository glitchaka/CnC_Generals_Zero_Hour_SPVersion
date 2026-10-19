#define wWinMain legacy_playable_wWinMain
#define playableWnd legacy_playableWnd
#define updatePlayable legacy_updatePlayable
#define renderPlayable legacy_renderPlayable
#include "demo_d3d11_playable.cpp"
#undef renderPlayable
#undef updatePlayable
#undef playableWnd
#undef wWinMain

#include <cstdio>
#include <array>

struct HudVertex{float x,y;float r,g,b,a;};
static ID3D11VertexShader* g_hudVS2=nullptr;
static ID3D11PixelShader* g_hudPS2=nullptr;
static ID3D11InputLayout* g_hudLayout2=nullptr;
static ID3D11Buffer* g_hudVB2=nullptr;
static ID3D11BlendState* g_hudBlend2=nullptr;
static bool g_rmb=false,g_mmb=false;
static POINT g_lastMouse{};
static float g_camPitch=.62f;

static uint8_t glyphRow(char c,int row){
    c=(char)toupper((unsigned char)c);
    struct G{char c;std::array<uint8_t,7> r;};
    static const G g[]={
        {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},
        {'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
        {'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},{'I',{31,4,4,4,4,4,31}},
        {'J',{7,2,2,2,18,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
        {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},
        {'P',{30,17,17,30,16,16,16}},{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
        {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},
        {'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
        {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
        {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},
        {'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
        {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},
        {'9',{14,17,17,15,1,1,14}},{'.',{0,0,0,0,0,12,12}},{':',{0,12,12,0,12,12,0}},{'-',{0,0,0,31,0,0,0}},
        {'/',{1,2,4,8,16,0,0}},{' ',{0,0,0,0,0,0,0}}
    };
    for(const auto&x:g)if(x.c==c)return x.r[row];
    return 0;
}
static void hudRect(std::vector<HudVertex>&v,float x,float y,float w,float h,float r,float g,float b,float a){
    float l=2.f*x/g_w-1.f, rr=2.f*(x+w)/g_w-1.f, t=1.f-2.f*y/g_h, bb=1.f-2.f*(y+h)/g_h;
    HudVertex q[6]={{l,t,r,g,b,a},{rr,t,r,g,b,a},{rr,bb,r,g,b,a},{l,t,r,g,b,a},{rr,bb,r,g,b,a},{l,bb,r,g,b,a}};
    v.insert(v.end(),q,q+6);
}
static void hudText(std::vector<HudVertex>&v,float x,float y,const char*s,float scale=2.f){
    float ox=x;
    for(;*s;++s){if(*s=='\n'){y+=9.f*scale;x=ox;continue;}for(int row=0;row<7;++row){uint8_t bits=glyphRow(*s,row);for(int col=0;col<5;++col)if(bits&(1<<(4-col)))hudRect(v,x+col*scale,y+row*scale,scale,scale,.93f,.96f,.91f,1.f);}x+=6.f*scale;}
}
static bool initHudD3D(){
    const char*vs="struct I{float2 p:POSITION;float4 c:COLOR;};struct O{float4 p:SV_POSITION;float4 c:COLOR;};O main(I i){O o;o.p=float4(i.p,0,1);o.c=i.c;return o;}";
    const char*ps="struct I{float4 p:SV_POSITION;float4 c:COLOR;};float4 main(I i):SV_TARGET{return i.c;}";
    ID3DBlob*bv=nullptr,*bp=nullptr;if(!compileShader(vs,"main","vs_5_0",&bv)||!compileShader(ps,"main","ps_5_0",&bp))return false;
    bool ok=SUCCEEDED(g_dev->CreateVertexShader(bv->GetBufferPointer(),bv->GetBufferSize(),nullptr,&g_hudVS2))&&SUCCEEDED(g_dev->CreatePixelShader(bp->GetBufferPointer(),bp->GetBufferSize(),nullptr,&g_hudPS2));
    D3D11_INPUT_ELEMENT_DESC il[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0}};
    ok=ok&&SUCCEEDED(g_dev->CreateInputLayout(il,2,bv->GetBufferPointer(),bv->GetBufferSize(),&g_hudLayout2));rel(bv);rel(bp);
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(HudVertex)*65536;bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;ok=ok&&SUCCEEDED(g_dev->CreateBuffer(&bd,nullptr,&g_hudVB2));
    D3D11_BLEND_DESC bl{};bl.RenderTarget[0].BlendEnable=TRUE;bl.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;bl.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;bl.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;bl.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;bl.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;bl.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;bl.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;ok=ok&&SUCCEEDED(g_dev->CreateBlendState(&bl,&g_hudBlend2));return ok;
}
static void drawHudD3D(){
    std::vector<HudVertex>v;v.reserve(16000);hudRect(v,14,14,620,68,.025f,.03f,.025f,.82f);
    char line[256];snprintf(line,sizeof(line),"WASD MOVER   MOUSE BORDE/MEDIO CAM   RMB ROTAR   RUEDA ZOOM   F CENTRAR\nVEL %.2f   RIG %.2f   CAM %s   Z %.2f",len3(g_comVel),rheologyFactor(),g_followCamera?"SIGUE":"LIBRE",g_zoom);hudText(v,27,26,line,1.55f);
    if(v.empty())return;D3D11_MAPPED_SUBRESOURCE ms{};if(FAILED(g_ctx->Map(g_hudVB2,0,D3D11_MAP_WRITE_DISCARD,0,&ms)))return;memcpy(ms.pData,v.data(),v.size()*sizeof(HudVertex));g_ctx->Unmap(g_hudVB2,0);
    UINT st=sizeof(HudVertex),off=0;g_ctx->IASetInputLayout(g_hudLayout2);g_ctx->IASetVertexBuffers(0,1,&g_hudVB2,&st,&off);g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);g_ctx->VSSetShader(g_hudVS2,nullptr,0);g_ctx->PSSetShader(g_hudPS2,nullptr,0);float bf[4]={0,0,0,0};g_ctx->OMSetBlendState(g_hudBlend2,bf,0xffffffff);g_ctx->OMSetDepthStencilState(nullptr,0);g_ctx->Draw((UINT)v.size(),0);g_ctx->OMSetBlendState(nullptr,bf,0xffffffff);
}

static void applyDriveMouse(float ix,float iz){
    float m=sqrtf(ix*ix+iz*iz);if(m<.001f)return;ix/=m;iz/=m;computeCOM();XMFLOAT3 axis=norm3({iz,0,-ix});
    std::vector<XMFLOAT3>forces(g_surfaceCount);XMFLOAT3 sum{0,0,0};const float omega=1.55f,gain=15.0f;
    for(uint32_t i=0;i<g_surfaceCount;++i){XMFLOAT3 r=sub3(g_nodes[i].p,g_com);XMFLOAT3 desired=mul3(cross3(axis,r),omega);XMFLOAT3 relv=sub3(g_nodes[i].v,g_comVel);XMFLOAT3 err=sub3(desired,relv);float surf=clampf(len3(r)/1.55f,.35f,1.f);forces[i]=mul3(err,gain*surf);sum=add3(sum,forces[i]);}
    XMFLOAT3 mean=mul3(sum,1.f/float(g_surfaceCount));for(uint32_t i=0;i<g_surfaceCount;++i)g_nodes[i].f=add3(g_nodes[i].f,sub3(forces[i],mean));
}
static void physicsStepMouse(float dt,float ix,float iz){
    g_hardening=(std::max)(0.f,g_hardening-dt*.62f);float stiff=rheologyFactor();
    for(auto&n:g_nodes)n.f={-.34f*n.v.x,-9.81f*n.mass-.34f*n.v.y,-.34f*n.v.z};
    applySpringForces(1.18f*stiff);applyVolumePressure(1.28f*stiff);applyCohesionPlayable(1.22f*stiff);applyDriveMouse(ix,iz);
    for(size_t i=0;i<g_nodes.size();++i){Node&n=g_nodes[i];XMFLOAT3 a=mul3(n.f,1.f/n.mass);n.v=add3(n.v,mul3(a,dt));float sp=len3(n.v);if(sp>7.2f)n.v=mul3(n.v,7.2f/sp);n.p=add3(n.p,mul3(n.v,dt));if(i<g_surfaceCount){collideGround(n,dt);for(const auto&o:g_scene)collideObject(n,o);}}computeCOM();
}
static void panCamera(float sx,float sy){float k=.030f*g_zoom;g_followCamera=false;g_camPanX+=(cosf(g_camYaw)*sx+sinf(g_camYaw)*sy)*k;g_camPanZ+=(-sinf(g_camYaw)*sx+cosf(g_camYaw)*sy)*k;}
static void updateMouseCamera(float dt){
    POINT p{};GetCursorPos(&p);ScreenToClient(g_hwnd,&p);const int edge=14;float dx=0,dy=0;if(p.x>=0&&p.x<edge)dx-=1;if(p.x>=g_w-edge&&p.x<g_w)dx+=1;if(p.y>=0&&p.y<edge)dy+=1;if(p.y>=g_h-edge&&p.y<g_h)dy-=1;if(dx||dy)panCamera(dx*520.f*dt,dy*520.f*dt);
}
static void updatePlayableMouse(float dt){
    float side=0,fwd=0;if(g_keys['A'])side-=1;if(g_keys['D'])side+=1;if(g_keys['W'])fwd+=1;if(g_keys['S'])fwd-=1;if(side&&fwd){side*=.7071067f;fwd*=.7071067f;}
    float ix=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,iz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd;
    const int sub=14;float h=dt/sub;for(int i=0;i<sub;++i)physicsStepMouse(h,ix,iz);updateBlobMesh();updateMouseCamera(dt);
    if(g_followCamera){float k=1.f-expf(-5.5f*dt);g_cam.x+=(g_com.x-g_cam.x)*k;g_cam.z+=(g_com.z-g_cam.z)*k;g_camPanX=g_camPanZ=0;}else{g_cam.x=g_com.x+g_camPanX;g_cam.z=g_com.z+g_camPanZ;}g_cam.y=groundH(g_cam.x,g_cam.z)+1.05f;
}
static void renderPlayableMouse(){
    if(!g_rtv)return;float clear[]={.14f,.16f,.145f,1};g_ctx->OMSetRenderTargets(1,&g_rtv,g_dsv);g_ctx->ClearRenderTargetView(g_rtv,clear);g_ctx->ClearDepthStencilView(g_dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);D3D11_VIEWPORT vp={0,0,(float)g_w,(float)g_h,0,1};g_ctx->RSSetViewports(1,&vp);g_ctx->IASetInputLayout(g_layout);g_ctx->VSSetShader(g_vs,nullptr,0);g_ctx->PSSetShader(g_ps,nullptr,0);drawMeshPlayable(g_terrain,XMMatrixIdentity(),{1.02f,1.02f,1.02f,1},1,g_groundSRV);for(const auto&o:g_scene)drawMeshPlayable(*o.mesh,objectWorld(o),{1.10f,1.10f,1.10f,1},1,g_kaySRV);drawMeshPlayable(g_blob,XMMatrixIdentity(),{.16f,.018f,.03f,1},0,nullptr);drawHudD3D();g_swap->Present(1,0);
}
static LRESULT CALLBACK playableWndMouse(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);return p.y<7?HTCAPTION:HTCLIENT;}
        case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;
        case WM_MOUSEWHEEL:g_zoom*=GET_WHEEL_DELTA_WPARAM(w)>0?.90f:1.11f;g_zoom=clampf(g_zoom,.48f,2.8f);return 0;
        case WM_RBUTTONDOWN:g_rmb=true;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
        case WM_MBUTTONDOWN:g_mmb=true;g_followCamera=false;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
        case WM_MOUSEMOVE:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int dx=p.x-g_lastMouse.x,dy=p.y-g_lastMouse.y;if(g_rmb){g_camYaw+=dx*.0065f;g_camPitch=clampf(g_camPitch+dy*.0045f,.32f,1.05f);}if(g_mmb)panCamera((float)-dx,(float)dy);if(g_rmb||g_mmb)g_lastMouse=p;return 0;}
        case WM_RBUTTONUP:g_rmb=false;if(!g_mmb)ReleaseCapture();return 0;
        case WM_MBUTTONUP:g_mmb=false;if(!g_rmb)ReleaseCapture();return 0;
        case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='F'){g_followCamera=true;g_camPanX=g_camPanZ=0;}if(w==VK_ESCAPE)DestroyWindow(h);return 0;
        case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;
        case WM_KILLFOCUS:for(bool&k:g_keys)k=false;g_rmb=g_mmb=false;return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,m,w,l);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);WNDCLASSW wc{};wc.hInstance=hi;wc.lpfnWndProc=playableWndMouse;wc.lpszClassName=L"SP_D3D11_MOUSE";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);RECT wa{};SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);int aw=wa.right-wa.left,ah=wa.bottom-wa.top;g_w=(std::min)(1440,(std::max)(900,aw-80));g_h=(std::min)(900,(std::max)(650,ah-80));g_w=(std::min)(g_w,aw);g_h=(std::min)(g_h,ah);int x=wa.left+(aw-g_w)/2,y=wa.top+(ah-g_h)/2;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;
    if(!initD3D()||!initHudD3D()){MessageBoxW(g_hwnd,L"No pude iniciar Direct3D/HUD.",L"SPVersion",MB_ICONERROR);return 2;}buildTerrainPlayable();bool ok=true;ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\rubble_large.obj",g_rubble);ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\crates_stacked.obj",g_crates);ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\barrel_large.obj",g_barrel);if(!ok){MessageBoxW(g_hwnd,L"No pude cargar la geometria KayKit.",L"SPVersion",MB_ICONERROR);return 3;}buildScenePlayable();buildBlob();initRestShape();g_cam=g_com;
    LARGE_INTEGER fq,p,n;QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&p);MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}QueryPerformanceCounter(&n);float dt=(float)((double)(n.QuadPart-p.QuadPart)/fq.QuadPart);p=n;dt=(std::min)(dt,.016f);syncClientSize();updatePlayableMouse(dt);renderPlayableMouse();Sleep(1);}killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_blob);killMesh(g_terrain);rel(g_hudBlend2);rel(g_hudVB2);rel(g_hudLayout2);rel(g_hudPS2);rel(g_hudVS2);rel(g_kaySRV);rel(g_groundSRV);rel(g_sampler);rel(g_cb);rel(g_layout);rel(g_ps);rel(g_vs);destroySize();rel(g_swap);rel(g_ctx);rel(g_dev);CoUninitialize();return 0;
}
