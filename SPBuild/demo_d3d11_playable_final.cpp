#include <cctype>
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#define wWinMain mouse_stage_wWinMain
#define playableWndMouse mouse_stage_wnd
#define renderPlayableMouse mouse_stage_render
#include "demo_d3d11_playable_mouse.cpp"
#undef renderPlayableMouse
#undef playableWndMouse
#undef wWinMain

static void drawMeshFinal(const MeshGPU&m,const XMMATRIX&w,const XMFLOAT4&c,float useTex,ID3D11ShaderResourceView*srv){
    if(!m.vb||!m.ib||!m.count)return;UINT st=sizeof(Vertex),off=0;g_ctx->IASetVertexBuffers(0,1,&m.vb,&st,&off);g_ctx->IASetIndexBuffer(m.ib,DXGI_FORMAT_R32_UINT,0);g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    float orbit=31.f*g_zoom;float horiz=orbit*cosf(g_camPitch),height=orbit*sinf(g_camPitch);
    XMVECTOR eye=XMVectorSet(g_cam.x-sinf(g_camYaw)*horiz,g_cam.y+height,g_cam.z-cosf(g_camYaw)*horiz,1),at=XMVectorSet(g_cam.x,g_cam.y,g_cam.z,1),up=XMVectorSet(0,1,0,0);
    XMMATRIX view=XMMatrixLookAtLH(eye,at,up),proj=XMMatrixPerspectiveFovLH(XMConvertToRadians(43.f),g_w/(float)(std::max)(1,g_h),.1f,650.f);CB cb{};cb.world=XMMatrixTranspose(w);cb.view=XMMatrixTranspose(view);cb.proj=XMMatrixTranspose(proj);cb.color=c;cb.lightDir={-.28f,-1.f,.18f};cb.useTex=useTex;g_ctx->UpdateSubresource(g_cb,0,nullptr,&cb,0,0);g_ctx->VSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetShaderResources(0,1,&srv);g_ctx->PSSetSamplers(0,1,&g_sampler);g_ctx->DrawIndexed(m.count,0,0);
}
static void renderFinal(){
    if(!g_rtv)return;float clear[]={.14f,.16f,.145f,1};g_ctx->OMSetRenderTargets(1,&g_rtv,g_dsv);g_ctx->ClearRenderTargetView(g_rtv,clear);g_ctx->ClearDepthStencilView(g_dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);D3D11_VIEWPORT vp={0,0,(float)g_w,(float)g_h,0,1};g_ctx->RSSetViewports(1,&vp);g_ctx->IASetInputLayout(g_layout);g_ctx->VSSetShader(g_vs,nullptr,0);g_ctx->PSSetShader(g_ps,nullptr,0);drawMeshFinal(g_terrain,XMMatrixIdentity(),{1.02f,1.02f,1.02f,1},1,g_groundSRV);for(const auto&o:g_scene)drawMeshFinal(*o.mesh,objectWorld(o),{1.10f,1.10f,1.10f,1},1,g_kaySRV);drawMeshFinal(g_blob,XMMatrixIdentity(),{.16f,.018f,.03f,1},0,nullptr);drawHudD3D();g_swap->Present(1,0);
}
static LRESULT CALLBACK finalWnd(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);return p.y<7?HTCAPTION:HTCLIENT;}
        case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;
        case WM_MOUSEWHEEL:g_zoom*=GET_WHEEL_DELTA_WPARAM(w)>0?.90f:1.11f;g_zoom=clampf(g_zoom,.48f,2.8f);return 0;
        case WM_RBUTTONDOWN:g_rmb=true;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
        case WM_MBUTTONDOWN:g_mmb=true;g_followCamera=false;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
        case WM_MOUSEMOVE:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int dx=p.x-g_lastMouse.x,dy=p.y-g_lastMouse.y;if(g_rmb){g_camYaw+=dx*.0065f;g_camPitch=clampf(g_camPitch+dy*.0045f,.30f,1.12f);}if(g_mmb)panCamera((float)-dx,(float)dy);if(g_rmb||g_mmb)g_lastMouse=p;return 0;}
        case WM_RBUTTONUP:g_rmb=false;if(!g_mmb)ReleaseCapture();return 0;
        case WM_MBUTTONUP:g_mmb=false;if(!g_rmb)ReleaseCapture();return 0;
        case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='F'){g_followCamera=true;g_camPanX=g_camPanZ=0;}if(w==VK_ESCAPE)DestroyWindow(h);return 0;
        case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;
        case WM_KILLFOCUS:for(bool&k:g_keys)k=false;g_rmb=g_mmb=false;return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);WNDCLASSW wc{};wc.hInstance=hi;wc.lpfnWndProc=finalWnd;wc.lpszClassName=L"SP_D3D11_FINAL";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);RECT wa{};SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);int aw=wa.right-wa.left,ah=wa.bottom-wa.top;g_w=(std::min)(1440,(std::max)(900,aw-80));g_h=(std::min)(900,(std::max)(650,ah-80));g_w=(std::min)(g_w,aw);g_h=(std::min)(g_h,ah);int x=wa.left+(aw-g_w)/2,y=wa.top+(ah-g_h)/2;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;
    if(!initD3D()||!initHudD3D()){MessageBoxW(g_hwnd,L"No pude iniciar Direct3D/HUD.",L"SPVersion",MB_ICONERROR);return 2;}buildTerrainPlayable();bool ok=true;ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\rubble_large.obj",g_rubble);ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\crates_stacked.obj",g_crates);ok&=loadOBJ(L"ExternalAssets\\KayKitDungeon\\barrel_large.obj",g_barrel);if(!ok){MessageBoxW(g_hwnd,L"No pude cargar la geometria KayKit.",L"SPVersion",MB_ICONERROR);return 3;}buildScenePlayable();buildBlob();initRestShape();g_cam=g_com;
    LARGE_INTEGER fq,p,n;QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&p);MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}QueryPerformanceCounter(&n);float dt=(float)((double)(n.QuadPart-p.QuadPart)/fq.QuadPart);p=n;dt=(std::min)(dt,.016f);syncClientSize();updatePlayableMouse(dt);renderFinal();Sleep(1);}killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_blob);killMesh(g_terrain);rel(g_hudBlend2);rel(g_hudVB2);rel(g_hudLayout2);rel(g_hudPS2);rel(g_hudVS2);rel(g_kaySRV);rel(g_groundSRV);rel(g_sampler);rel(g_cb);rel(g_layout);rel(g_ps);rel(g_vs);destroySize();rel(g_swap);rel(g_ctx);rel(g_dev);CoUninitialize();return 0;
}
