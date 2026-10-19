#define wWinMain legacy_wWinMain
#define wnd legacy_wnd
#include "demo_d3d11_nonnewtonian.cpp"
#undef wnd
#undef wWinMain

#include <cwchar>

static HWND g_hudWnd=nullptr;
static std::wstring g_hudText;
static bool g_followCamera=true;
static float g_camYaw=.72f;
static float g_camPanX=0.f,g_camPanZ=0.f;
static float g_hudTick=0.f;

static void initRestShape(){
    computeCOM();
    float sx=0,sy=0,sz=0;
    for(uint32_t i=0;i<g_surfaceCount;++i){XMFLOAT3 r=sub3(g_nodes[i].p,g_com);sx+=r.x*r.x;sy+=r.y*r.y;sz+=r.z*r.z;}
    float inv=1.f/float((std::max)(1u,g_surfaceCount));
    g_restRms={sqrtf(sx*inv),sqrtf(sy*inv),sqrtf(sz*inv)};
}

static void buildTerrainPlayable(){
    killMesh(g_terrain);
    const int N=257; const float SIZE=420.f,step=SIZE/(N-1),half=SIZE*.5f;
    std::vector<Vertex>v; std::vector<uint32_t>i; v.reserve(N*N); i.reserve((N-1)*(N-1)*6);
    for(int z=0;z<N;++z)for(int x=0;x<N;++x){float wx=-half+x*step,wz=-half+z*step;v.push_back({{wx,groundH(wx,wz),wz},terrainNormal(wx,wz),{wx*.075f,wz*.075f}});}
    for(int z=0;z<N-1;++z)for(int x=0;x<N-1;++x){uint32_t a=z*N+x;i.insert(i.end(),{a,a+N,a+1,a+1,a+N,a+N+1});}
    makeMesh(v,i,g_terrain,false);
}

static void buildScenePlayable(){
    g_scene.clear(); auto add=[&](MeshGPU&m,float x,float z,float yaw,float s,bool c=true){g_scene.push_back({&m,x,z,yaw,s,c});};
    add(g_crates,-52,44,.25f,1.45f); add(g_barrel,-36,48,-.5f,1.5f); add(g_rubble,-18,41,.8f,1.55f); add(g_barrel,8,49,.15f,1.45f); add(g_crates,36,43,-.65f,1.35f); add(g_rubble,58,33,-.8f,1.45f);
    add(g_rubble,-62,20,1.7f,1.25f); add(g_barrel,-40,17,.2f,1.55f); add(g_crates,-15,19,.55f,1.25f); add(g_rubble,16,23,2.2f,1.30f); add(g_crates,43,18,-.3f,1.40f); add(g_barrel,64,12,.4f,1.45f);
    add(g_rubble,-68,-8,.3f,1.6f); add(g_crates,-46,-13,.7f,1.3f); add(g_barrel,-20,-16,-.2f,1.55f); add(g_rubble,11,-12,1.1f,1.35f); add(g_crates,39,-19,-.45f,1.45f); add(g_barrel,65,-23,.1f,1.50f);
    add(g_rubble,-58,-46,.4f,1.45f); add(g_barrel,-31,-42,-.2f,1.45f); add(g_crates,-5,-49,.4f,1.30f); add(g_rubble,25,-42,-.7f,1.55f); add(g_barrel,49,-47,.6f,1.50f); add(g_crates,70,-40,-.2f,1.35f);
    add(g_rubble,-80,62,.2f,1.40f); add(g_crates,79,61,-.6f,1.35f); add(g_barrel,-79,-65,.2f,1.5f); add(g_rubble,80,-67,.9f,1.45f);
}

static void applyCohesionPlayable(float stiff){
    computeCOM();
    float sx=0,sy=0,sz=0;
    for(uint32_t i=0;i<g_surfaceCount;++i){XMFLOAT3 r=sub3(g_nodes[i].p,g_com);sx+=r.x*r.x;sy+=r.y*r.y;sz+=r.z*r.z;}
    float inv=1.f/float((std::max)(1u,g_surfaceCount)); sx=sqrtf(sx*inv); sy=sqrtf(sy*inv); sz=sqrtf(sz*inv);
    float yLoss=clampf((g_restRms.y-sy)/(g_restRms.y+.001f),-.7f,.9f);
    float spread=((sx+sz)-(g_restRms.x+g_restRms.z))/(g_restRms.x+g_restRms.z+.001f);
    float compactK=36.f+10.f*stiff, liftK=118.f+22.f*stiff;
    for(uint32_t i=0;i<g_surfaceCount;++i){
        XMFLOAT3 r=sub3(g_nodes[i].p,g_com); float rl=len3(r); if(rl<1e-4f)continue;
        XMFLOAT3 radial=mul3(r,1.f/rl);
        g_nodes[i].f=add3(g_nodes[i].f,mul3(radial,-compactK*spread));
        float yWeight=.20f+.80f*clampf(fabsf(r.y)/(g_restRms.y+.01f),0.f,1.f);
        g_nodes[i].f.y += (r.y>=0?1.f:-1.f)*liftK*yLoss*yWeight;
    }
}

static void applyDrivePlayable(float ix,float iz){
    float mag=sqrtf(ix*ix+iz*iz); if(mag<.001f)return; ix/=mag; iz/=mag; computeCOM();
    XMFLOAT3 axis=norm3({iz,0,-ix});
    std::vector<uint32_t> contact,upper; contact.reserve(g_surfaceCount/4); upper.reserve(g_surfaceCount/3);
    for(uint32_t i=0;i<g_surfaceCount;++i){float floorY=groundH(g_nodes[i].p.x,g_nodes[i].p.z);XMFLOAT3 r=sub3(g_nodes[i].p,g_com);if(g_nodes[i].p.y-floorY<.16f)contact.push_back(i);if(r.y>.30f)upper.push_back(i);}
    const float torque=10.5f;
    for(uint32_t i=0;i<g_surfaceCount;++i){XMFLOAT3 r=sub3(g_nodes[i].p,g_com);g_nodes[i].f=add3(g_nodes[i].f,mul3(cross3(axis,r),torque));}
    if(!contact.empty()&&!upper.empty()){
        const float traction=85.f; float cf=traction/float(contact.size()),uf=traction/float(upper.size());
        XMFLOAT3 back={-ix*cf,0,-iz*cf},front={ix*uf,0,iz*uf};
        for(auto i:contact)g_nodes[i].f=add3(g_nodes[i].f,back);
        for(auto i:upper)g_nodes[i].f=add3(g_nodes[i].f,front);
    }
}

static void physicsStepPlayable(float dt,float ix,float iz){
    g_hardening=(std::max)(0.f,g_hardening-dt*.72f); float stiff=rheologyFactor();
    for(auto&n:g_nodes)n.f={-.24f*n.v.x,-9.81f*n.mass-.24f*n.v.y,-.24f*n.v.z};
    applySpringForces(stiff); applyVolumePressure(stiff); applyCohesionPlayable(stiff); applyDrivePlayable(ix,iz);
    for(size_t i=0;i<g_nodes.size();++i){Node&n=g_nodes[i];XMFLOAT3 a=mul3(n.f,1.f/n.mass);n.v=add3(n.v,mul3(a,dt));float sp=len3(n.v);if(sp>9.f)n.v=mul3(n.v,9.f/sp);n.p=add3(n.p,mul3(n.v,dt));if(i<g_surfaceCount){collideGround(n,dt);for(const auto&o:g_scene)collideObject(n,o);}}
    computeCOM();
}

static void updatePlayable(float dt){
    float ix=0,iz=0;if(g_keys['A'])ix-=1;if(g_keys['D'])ix+=1;if(g_keys['W'])iz+=1;if(g_keys['S'])iz-=1;if(ix&&iz){ix*=.7071067f;iz*=.7071067f;}
    const int sub=12;float h=dt/sub;for(int i=0;i<sub;++i)physicsStepPlayable(h,ix,iz);updateBlobMesh();
    float pan=26.f*dt*g_zoom; float fwd=(g_keys[VK_UP]?1.f:0.f)-(g_keys[VK_DOWN]?1.f:0.f);float side=(g_keys[VK_RIGHT]?1.f:0.f)-(g_keys[VK_LEFT]?1.f:0.f);
    if(g_keys['Q'])g_camYaw-=1.45f*dt;if(g_keys['E'])g_camYaw+=1.45f*dt;
    if(fwd||side){g_followCamera=false;g_camPanX+=(cosf(g_camYaw)*side+sinf(g_camYaw)*fwd)*pan;g_camPanZ+=(-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd)*pan;}
    if(g_followCamera){float k=1.f-expf(-4.5f*dt);g_cam.x+=(g_com.x-g_cam.x)*k;g_cam.z+=(g_com.z-g_cam.z)*k;}else{g_cam.x=g_com.x+g_camPanX;g_cam.z=g_com.z+g_camPanZ;}
    g_cam.y=groundH(g_cam.x,g_cam.z)+1.2f;
    g_hudTick+=dt;if(g_hudTick>.08f){g_hudTick=0.f;wchar_t b[512];swprintf_s(b,L"WASD mover masa   Flechas mover camara   Q/E rotar   Rueda zoom   F seguir/recentrar\r\nVel %.2f   Rigidez %.2f   Camara %s   Zoom %.2f",len3(g_comVel),rheologyFactor(),g_followCamera?L"SIGUIENDO":L"LIBRE",g_zoom);g_hudText=b;if(g_hudWnd)InvalidateRect(g_hudWnd,nullptr,TRUE);}
}

static void drawMeshPlayable(const MeshGPU&m,const XMMATRIX&w,const XMFLOAT4&c,float useTex,ID3D11ShaderResourceView*srv){
    if(!m.vb||!m.ib||!m.count)return;UINT st=sizeof(Vertex),off=0;g_ctx->IASetVertexBuffers(0,1,&m.vb,&st,&off);g_ctx->IASetIndexBuffer(m.ib,DXGI_FORMAT_R32_UINT,0);g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    float dist=24.f*g_zoom,height=18.f*g_zoom;XMVECTOR eye=XMVectorSet(g_cam.x-sinf(g_camYaw)*dist,g_cam.y+height,g_cam.z-cosf(g_camYaw)*dist,1),at=XMVectorSet(g_cam.x,g_cam.y,g_cam.z,1),up=XMVectorSet(0,1,0,0);
    XMMATRIX view=XMMatrixLookAtLH(eye,at,up),proj=XMMatrixPerspectiveFovLH(XMConvertToRadians(44.f),g_w/(float)(std::max)(1,g_h),.1f,500.f);CB cb{};cb.world=XMMatrixTranspose(w);cb.view=XMMatrixTranspose(view);cb.proj=XMMatrixTranspose(proj);cb.color=c;cb.lightDir={-.28f,-1.f,.18f};cb.useTex=useTex;g_ctx->UpdateSubresource(g_cb,0,nullptr,&cb,0,0);g_ctx->VSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetShaderResources(0,1,&srv);g_ctx->PSSetSamplers(0,1,&g_sampler);g_ctx->DrawIndexed(m.count,0,0);
}

static void renderPlayable(){
    if(!g_rtv)return;float clear[]={.16f,.18f,.16f,1};g_ctx->OMSetRenderTargets(1,&g_rtv,g_dsv);g_ctx->ClearRenderTargetView(g_rtv,clear);g_ctx->ClearDepthStencilView(g_dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);D3D11_VIEWPORT vp={0,0,(float)g_w,(float)g_h,0,1};g_ctx->RSSetViewports(1,&vp);g_ctx->IASetInputLayout(g_layout);g_ctx->VSSetShader(g_vs,nullptr,0);g_ctx->PSSetShader(g_ps,nullptr,0);
    drawMeshPlayable(g_terrain,XMMatrixIdentity(),{1.02f,1.02f,1.02f,1},1,g_groundSRV);for(const auto&o:g_scene)drawMeshPlayable(*o.mesh,objectWorld(o),{1.12f,1.12f,1.12f,1},1,g_kaySRV);drawMeshPlayable(g_blob,XMMatrixIdentity(),{.20f,.025f,.035f,1},0,nullptr);g_swap->Present(1,0);
}

static LRESULT CALLBACK hudProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);HBRUSH br=CreateSolidBrush(RGB(12,14,12));FillRect(dc,&r,br);DeleteObject(br);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(238,242,234));HFONT f=(HFONT)GetStockObject(DEFAULT_GUI_FONT);SelectObject(dc,f);RECT tr{12,8,r.right-8,r.bottom-4};DrawTextW(dc,g_hudText.c_str(),-1,&tr,DT_LEFT|DT_TOP|DT_NOPREFIX);EndPaint(h,&ps);return 0;}return DefWindowProcW(h,m,w,l);}

static LRESULT CALLBACK playableWnd(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){case WM_ERASEBKGND:return 1;case WM_NCHITTEST:{LRESULT r=DefWindowProcW(h,m,w,l);return r==HTCLIENT?HTCAPTION:r;}case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED){resizeSwap(LOWORD(l),HIWORD(l));if(g_hudWnd)MoveWindow(g_hudWnd,12,12,(std::min)(760,g_w-24),64,TRUE);}return 0;case WM_MOUSEWHEEL:g_zoom*=GET_WHEEL_DELTA_WPARAM(w)>0?.90f:1.11f;g_zoom=clampf(g_zoom,.48f,2.8f);return 0;case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='F'){g_followCamera=true;g_camPanX=g_camPanZ=0;}if(w==VK_ESCAPE)DestroyWindow(h);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(bool&k:g_keys)k=false;return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);
}

#ifndef SP_PLAYABLE_NO_ENTRYPOINT
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);WNDCLASSW wc{};wc.hInstance=hi;wc.lpfnWndProc=playableWnd;wc.lpszClassName=L"SP_D3D11_PLAYABLE";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);WNDCLASSW hc{};hc.hInstance=hi;hc.lpfnWndProc=hudProc;hc.lpszClassName=L"SP_HUD";hc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&hc);
    RECT wa{};SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);int aw=wa.right-wa.left,ah=wa.bottom-wa.top;g_w=(std::min)(1440,(std::max)(900,aw-80));g_h=(std::min)(900,(std::max)(650,ah-80));g_w=(std::min)(g_w,aw);g_h=(std::min)(g_h,ah);int x=wa.left+(aw-g_w)/2,y=wa.top+(ah-g_h)/2;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;
    g_hudText=L"WASD mover masa   Flechas mover camara   Q/E rotar   Rueda zoom   F seguir/recentrar";g_hudWnd=CreateWindowExW(0,hc.lpszClassName,L"",WS_CHILD|WS_VISIBLE,12,12,(std::min)(760,g_w-24),64,g_hwnd,nullptr,hi,nullptr);
    if(!initD3D()){MessageBoxW(g_hwnd,L"No pude cargar las texturas requeridas.",L"SPVersion",MB_ICONERROR);return 2;}buildTerrainPlayable();bool ok=true;ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\rubble_large.obj",g_rubble);ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\crates_stacked.obj",g_crates);ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\barrel_large.obj",g_barrel);if(!ok){MessageBoxW(g_hwnd,L"No pude cargar la geometria KayKit incluida en el proyecto.",L"SPVersion",MB_ICONERROR);return 3;}buildScenePlayable();buildBlob();initRestShape();g_cam=g_com;
    LARGE_INTEGER fq,p,n;QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&p);MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}QueryPerformanceCounter(&n);float dt=(float)((double)(n.QuadPart-p.QuadPart)/fq.QuadPart);p=n;dt=(std::min)(dt,.018f);syncClientSize();updatePlayable(dt);renderPlayable();Sleep(1);}killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_blob);killMesh(g_terrain);rel(g_kaySRV);rel(g_groundSRV);rel(g_sampler);rel(g_cb);rel(g_layout);rel(g_ps);rel(g_vs);destroySize();rel(g_swap);rel(g_ctx);rel(g_dev);CoUninitialize();return 0;
}
#endif
