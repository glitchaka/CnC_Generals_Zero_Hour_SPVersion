cmake_minimum_required(VERSION 3.24)
project(SPVersionCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sp_smoke smoke_main.cpp)

if(WIN32)
    set(CGLTF_DIR "${CMAKE_BINARY_DIR}/_deps/cgltf")
    set(CGLTF_HEADER "${CGLTF_DIR}/cgltf.h")
    file(MAKE_DIRECTORY "${CGLTF_DIR}")
    if(NOT EXISTS "${CGLTF_HEADER}")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/jkuhlmann/cgltf/85cd62382dfea638278962690cf515023f33ed00/cgltf.h"
            "${CGLTF_HEADER}"
            STATUS CGLTF_STATUS
            TLS_VERIFY ON)
        list(GET CGLTF_STATUS 0 CGLTF_CODE)
        if(NOT CGLTF_CODE EQUAL 0)
            message(FATAL_ERROR "No se pudo descargar cgltf.h: ${CGLTF_STATUS}")
        endif()
    endif()

    set(SP_DEMO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/demo_d3d11_human.cpp")
    set(SP_DEMO_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/demo_d3d11_human_release.cpp")
    file(READ "${SP_DEMO_SOURCE}" SP_DEMO_CODE)

    # Correct glTF -> DirectXMath matrix convention. The old conversion put
    # translation in the wrong place and sent skinned vertices toward infinity.
    set(SP_GLTF_MATRIX_BROKEN "static XMMATRIX gltfMatrixRow(const float* m){ return XMMATRIX(m[0],m[4],m[8],m[12], m[1],m[5],m[9],m[13], m[2],m[6],m[10],m[14], m[3],m[7],m[11],m[15]); }")
    set(SP_GLTF_MATRIX_FIXED  "static XMMATRIX gltfMatrixRow(const float* m){ return XMMATRIX(m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7], m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15]); }")
    string(REPLACE "${SP_GLTF_MATRIX_BROKEN}" "${SP_GLTF_MATRIX_FIXED}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Gameplay state for the finished vertical slice.
    set(SP_CHAR_GLOBAL_OLD "static Character g_player,g_npc;")
    set(SP_CHAR_GLOBAL_NEW [=[static Character g_player,g_npc;
enum class GameState { Playing, Victory, Defeated };
static GameState g_gameState=GameState::Playing;
static float g_enemyHealth=1.f;
static float g_attackCooldown=0.f,g_dashCooldown=0.f,g_enemyAttackCooldown=0.f;
static float g_damageFlash=0.f,g_enemyHitFlash=0.f;
static bool g_attackQueued=false,g_spaceLatch=false;]=])
    string(REPLACE "${SP_CHAR_GLOBAL_OLD}" "${SP_CHAR_GLOBAL_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Solid scenery collision plus solid player/NPC collision.
    set(SP_RESOLVE_OLD [=[static void resolveCharacter(Character& c){
    const float cr=.42f; for(const auto& o:g_scene){ if(!o.collider||!o.mesh)continue; float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.42f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.42f,r=(std::max)(.55f,(std::max)(rx,rz)); float dx=c.pos.x-o.x,dz=c.pos.z-o.z,ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr); if(ds<rr&&ds>1e-6f){float d=sqrtf(ds),push=r+cr-d;c.pos.x+=dx/d*push;c.pos.z+=dz/d*push;} } c.pos.y=groundH(c.pos.x,c.pos.z);
}]=])
    set(SP_RESOLVE_NEW [=[static void resolveCharacter(Character& c){
    const float cr=.42f; for(const auto& o:g_scene){ if(!o.collider||!o.mesh)continue; float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.42f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.42f,r=(std::max)(.55f,(std::max)(rx,rz)); float dx=c.pos.x-o.x,dz=c.pos.z-o.z,ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr); if(ds<rr&&ds>1e-6f){float d=sqrtf(ds),push=r+cr-d;c.pos.x+=dx/d*push;c.pos.z+=dz/d*push;} } c.pos.y=groundH(c.pos.x,c.pos.z);
}

static void resolveCharacterPair(Character& a,Character& b){
    const float minD=.96f; float dx=a.pos.x-b.pos.x,dz=a.pos.z-b.pos.z,ds=dx*dx+dz*dz;
    if(ds>=minD*minD)return;
    if(ds<1e-8f){dx=1.f;dz=0.f;ds=1.f;}
    float d=sqrtf(ds),push=(minD-d)*.5f,nx=dx/d,nz=dz/d;
    a.pos.x+=nx*push;a.pos.z+=nz*push;b.pos.x-=nx*push;b.pos.z-=nz*push;
    resolveCharacter(a);resolveCharacter(b);
}]=])
    string(REPLACE "${SP_RESOLVE_OLD}" "${SP_RESOLVE_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Camera-relative movement, walk/run, dodge and a short-range RMB attack.
    set(SP_PLAYER_OLD [=[static void updatePlayer(float dt){
    float side=(g_keys['D']?1.f:0.f)-(g_keys['A']?1.f:0.f),fwd=(g_keys['W']?1.f:0.f)-(g_keys['S']?1.f:0.f); float m=sqrtf(side*side+fwd*fwd); if(m>1){side/=m;fwd/=m;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd; float dm=sqrtf(dx*dx+dz*dz); g_player.moving=dm>.01f; g_player.speed=g_player.moving?4.8f:0;
    if(g_player.moving){dx/=dm;dz/=dm;g_player.yaw=approachAngle(g_player.yaw,atan2f(dx,dz),10.f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;}
    resolveCharacter(g_player); poseCharacter(g_player,g_player.moving?"Running_A":"Idle_A",dt,g_player.moving?1.05f:1.f);
}]=])
    set(SP_PLAYER_NEW [=[static void updatePlayer(float dt){
    g_attackCooldown=(std::max)(0.f,g_attackCooldown-dt);g_dashCooldown=(std::max)(0.f,g_dashCooldown-dt);g_damageFlash=(std::max)(0.f,g_damageFlash-dt*2.8f);g_enemyHitFlash=(std::max)(0.f,g_enemyHitFlash-dt*4.f);
    if(g_gameState!=GameState::Playing){g_player.moving=false;poseCharacter(g_player,"Idle_A",dt);g_attackQueued=false;return;}
    float side=(g_keys['D']?1.f:0.f)-(g_keys['A']?1.f:0.f),fwd=(g_keys['W']?1.f:0.f)-(g_keys['S']?1.f:0.f); float m=sqrtf(side*side+fwd*fwd); if(m>1){side/=m;fwd/=m;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd; float dm=sqrtf(dx*dx+dz*dz); bool run=g_keys[VK_SHIFT]; g_player.moving=dm>.01f; g_player.speed=g_player.moving?(run?5.6f:3.35f):0.f;
    if(g_player.moving){dx/=dm;dz/=dm;g_player.yaw=approachAngle(g_player.yaw,atan2f(dx,dz),12.f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;}
    bool space=g_keys[VK_SPACE];
    if(space&&!g_spaceLatch&&g_dashCooldown<=0.f){float ddx=dx,ddz=dz;if(dm<=.01f){ddx=sinf(g_player.yaw);ddz=cosf(g_player.yaw);}g_player.pos.x+=ddx*2.15f;g_player.pos.z+=ddz*2.15f;g_dashCooldown=1.15f;}
    g_spaceLatch=space;
    if(g_attackQueued&&g_attackCooldown<=0.f){
        float ex=g_npc.pos.x-g_player.pos.x,ez=g_npc.pos.z-g_player.pos.z,d=sqrtf(ex*ex+ez*ez);g_attackCooldown=.48f;
        if(d<2.15f&&g_enemyHealth>0.f){float nx=ex/(std::max)(d,.001f),nz=ez/(std::max)(d,.001f);g_player.yaw=atan2f(nx,nz);g_enemyHealth=(std::max)(0.f,g_enemyHealth-.26f);g_npc.pos.x+=nx*.72f;g_npc.pos.z+=nz*.72f;g_enemyHitFlash=.28f;if(g_enemyHealth<=0.f)g_gameState=GameState::Victory;}
    }
    g_attackQueued=false;resolveCharacter(g_player);
    poseCharacter(g_player,g_player.moving?(run?"Running_A":"Walking_A"):"Idle_A",dt,g_player.moving?(run?1.05f:1.f):1.f);
}]=])
    string(REPLACE "${SP_PLAYER_OLD}" "${SP_PLAYER_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Enemy pursues, collides and attacks instead of wandering harmlessly.
    set(SP_NPC_OLD [=[static void updateNPC(float dt){
    g_npc.aiPhase+=dt*.28f; XMFLOAT3 target={g_npc.home.x+cosf(g_npc.aiPhase)*7.5f,0,g_npc.home.z+sinf(g_npc.aiPhase)*5.5f}; float dx=target.x-g_npc.pos.x,dz=target.z-g_npc.pos.z,d=sqrtf(dx*dx+dz*dz); g_npc.moving=d>.15f; g_npc.speed=g_npc.moving?2.4f:0;
    if(g_npc.moving){dx/=d;dz/=d;g_npc.yaw=approachAngle(g_npc.yaw,atan2f(dx,dz),6.f*dt);g_npc.pos.x+=dx*g_npc.speed*dt;g_npc.pos.z+=dz*g_npc.speed*dt;}
    resolveCharacter(g_npc); poseCharacter(g_npc,g_npc.moving?"Walking_A":"Idle_A",dt,1.f);
}]=])
    set(SP_NPC_NEW [=[static void updateNPC(float dt){
    if(g_gameState!=GameState::Playing||g_enemyHealth<=0.f){g_npc.moving=false;poseCharacter(g_npc,"Idle_A",dt);return;}
    g_enemyAttackCooldown=(std::max)(0.f,g_enemyAttackCooldown-dt);float dx=g_player.pos.x-g_npc.pos.x,dz=g_player.pos.z-g_npc.pos.z,d=sqrtf(dx*dx+dz*dz);g_npc.moving=d>1.28f;g_npc.speed=g_npc.moving?2.75f:0.f;
    if(d>.001f){dx/=d;dz/=d;g_npc.yaw=approachAngle(g_npc.yaw,atan2f(dx,dz),7.f*dt);}
    if(g_npc.moving){g_npc.pos.x+=dx*g_npc.speed*dt;g_npc.pos.z+=dz*g_npc.speed*dt;}
    else if(d<1.5f&&g_enemyAttackCooldown<=0.f){g_enemyAttackCooldown=.82f;g_player.health=(std::max)(0.f,g_player.health-.13f);g_player.pos.x+=dx*.55f;g_player.pos.z+=dz*.55f;g_damageFlash=.34f;if(g_player.health<=0.f)g_gameState=GameState::Defeated;}
    resolveCharacter(g_npc);poseCharacter(g_npc,g_npc.moving?"Running_A":"Idle_A",dt,g_npc.moving?.82f:1.f);
}]=])
    string(REPLACE "${SP_NPC_OLD}" "${SP_NPC_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Reset gives the demo a complete retry loop without restarting the process.
    set(SP_CAMERA_BLOCK [=[static void updateCamera(float dt){
    XMFLOAT3 want={g_player.pos.x,g_player.pos.y+1.15f,g_player.pos.z}; float k=1.f-expf(-8.f*dt); g_camTarget.x+=(want.x-g_camTarget.x)*k;g_camTarget.y+=(want.y-g_camTarget.y)*k;g_camTarget.z+=(want.z-g_camTarget.z)*k;
    float dist=12.5f*g_zoom,h=dist*sinf(g_camPitch),flat=dist*cosf(g_camPitch); g_camEye={g_camTarget.x-sinf(g_camYaw)*flat,g_camTarget.y+h,g_camTarget.z-cosf(g_camYaw)*flat};
}]=])
    set(SP_CAMERA_AND_RESET [=[static void updateCamera(float dt){
    XMFLOAT3 want={g_player.pos.x,g_player.pos.y+1.15f,g_player.pos.z}; float k=1.f-expf(-8.f*dt); g_camTarget.x+=(want.x-g_camTarget.x)*k;g_camTarget.y+=(want.y-g_camTarget.y)*k;g_camTarget.z+=(want.z-g_camTarget.z)*k;
    float dist=12.5f*g_zoom,h=dist*sinf(g_camPitch),flat=dist*cosf(g_camPitch); g_camEye={g_camTarget.x-sinf(g_camYaw)*flat,g_camTarget.y+h,g_camTarget.z-cosf(g_camYaw)*flat};
}

static void resetGame(){
    g_gameState=GameState::Playing;g_player.health=1.f;g_enemyHealth=1.f;g_attackCooldown=g_dashCooldown=g_enemyAttackCooldown=g_damageFlash=g_enemyHitFlash=0.f;g_attackQueued=false;g_spaceLatch=false;
    g_player.pos={0,groundH(0,0),0};g_player.home=g_player.pos;g_player.yaw=0.f;g_player.animTime=0.f;
    g_npc.pos={8,groundH(8,9),9};g_npc.home=g_npc.pos;g_npc.yaw=XM_PI;g_npc.animTime=0.f;
    g_camTarget={0,1,0};
}]=])
    string(REPLACE "${SP_CAMERA_BLOCK}" "${SP_CAMERA_AND_RESET}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Enemy bar, cooldown feedback, hit flash and clear victory/defeat overlay.
    set(SP_HUD_OLD [=[static void drawHUD(){
    float cx=g_w*.5f; drawUIQuad(g_uiPanel,cx-270.f,g_h-116.f,540.f,92.f,{1,1,1,.96f}); drawUIQuad(g_uiHealth,cx-240.f,g_h-94.f,210.f*g_player.health,28.f,{1,1,1,1});
    for(int i=0;i<5;++i)drawUIQuad(i==0?g_uiSlot:g_uiPanel,cx+4.f+i*52.f,g_h-96.f,46.f,46.f,{1,1,1,1});
    drawUIQuad(g_uiWhite,cx-7.f,g_h*.5f-1.f,14.f,2.f,{1,1,1,.75f}); drawUIQuad(g_uiWhite,cx-1.f,g_h*.5f-7.f,2.f,14.f,{1,1,1,.75f});
}]=])
    set(SP_HUD_NEW [=[static void drawHUD(){
    float cx=g_w*.5f;drawUIQuad(g_uiPanel,cx-270.f,g_h-116.f,540.f,92.f,{1,1,1,.96f});drawUIQuad(g_uiHealth,cx-240.f,g_h-94.f,210.f*g_player.health,28.f,{1,1,1,1});
    for(int i=0;i<5;++i)drawUIQuad(i==0?g_uiSlot:g_uiPanel,cx+4.f+i*52.f,g_h-96.f,46.f,46.f,{1,1,1,1});
    float ready=1.f-clampf(g_attackCooldown/.48f,0.f,1.f);drawUIQuad(g_uiWhite,cx+8.f,g_h-91.f,38.f,38.f*ready,{.95f,.95f,1.f,.72f});
    float dash=1.f-clampf(g_dashCooldown/1.15f,0.f,1.f);drawUIQuad(g_uiWhite,cx+60.f,g_h-91.f,38.f,38.f*dash,{.72f,.88f,1.f,.68f});
    drawUIQuad(g_uiPanel,cx-190.f,28.f,380.f,38.f,{1,1,1,.9f});drawUIQuad(g_uiWhite,cx-178.f,39.f,356.f*g_enemyHealth,16.f,{.88f,.20f,.16f,.95f});
    drawUIQuad(g_uiWhite,cx-7.f,g_h*.5f-1.f,14.f,2.f,{1,1,1,.72f});drawUIQuad(g_uiWhite,cx-1.f,g_h*.5f-7.f,2.f,14.f,{1,1,1,.72f});
    if(g_enemyHitFlash>0.f)drawUIQuad(g_uiWhite,cx-196.f,24.f,392.f,46.f,{1.f,.82f,.35f,g_enemyHitFlash});
    if(g_damageFlash>0.f)drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.95f,.05f,.03f,g_damageFlash*.32f});
    if(g_gameState!=GameState::Playing){XMFLOAT4 c=g_gameState==GameState::Victory?XMFLOAT4{.18f,.82f,.38f,.84f}:XMFLOAT4{.82f,.12f,.10f,.84f};drawUIQuad(g_uiPanel,cx-260.f,g_h*.5f-82.f,520.f,164.f,{1,1,1,.96f});drawUIQuad(g_uiWhite,cx-220.f,g_h*.5f-30.f,440.f,60.f,c);drawUIQuad(g_uiWhite,cx-95.f,g_h*.5f+48.f,190.f,5.f,{1,1,1,.8f});}
}]=])
    string(REPLACE "${SP_HUD_OLD}" "${SP_HUD_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Mouse/game-state input, DPI correctness and reliable capture sizing.
    set(SP_WMSIZE_OLD "        case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;")
    set(SP_WMSIZE_NEW [=[        case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;
        case WM_DPICHANGED:{RECT* r=(RECT*)l;SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}]=])
    string(REPLACE "${SP_WMSIZE_OLD}" "${SP_WMSIZE_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_LBUTTON_OLD [=[        case WM_LBUTTONDOWN:g_lmb=true;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
        case WM_LBUTTONUP:g_lmb=false;ReleaseCapture();return 0;]=])
    set(SP_LBUTTON_NEW [=[        case WM_LBUTTONDOWN:g_lmb=true;g_lastMouse={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
        case WM_LBUTTONUP:g_lmb=false;ReleaseCapture();return 0;
        case WM_RBUTTONDOWN:if(g_gameState==GameState::Playing)g_attackQueued=true;return 0;]=])
    string(REPLACE "${SP_LBUTTON_OLD}" "${SP_LBUTTON_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_KEYDOWN_OLD "        case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w==VK_ESCAPE)DestroyWindow(h);return 0;")
    set(SP_KEYDOWN_NEW "        case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='R'&&g_gameState!=GameState::Playing)resetGame();if(w==VK_ESCAPE)DestroyWindow(h);return 0;")
    string(REPLACE "${SP_KEYDOWN_OLD}" "${SP_KEYDOWN_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_MAIN_OLD "int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){\n    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);")
    set(SP_MAIN_NEW [=[int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){
    using SetDpiCtxFn=BOOL (WINAPI*)(HANDLE);HMODULE u32=GetModuleHandleW(L"user32.dll");SetDpiCtxFn setDpi=(SetDpiCtxFn)GetProcAddress(u32,"SetProcessDpiAwarenessContext");if(setDpi)setDpi((HANDLE)-4);else SetProcessDPIAware();
    wchar_t exePath[MAX_PATH]{};GetModuleFileNameW(nullptr,exePath,MAX_PATH);wchar_t* slash=wcsrchr(exePath,L'\\');if(slash){*slash=0;SetCurrentDirectoryW(exePath);}
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);]=])
    string(REPLACE "${SP_MAIN_OLD}" "${SP_MAIN_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_CREATE_OLD "g_hwnd=CreateWindowExW(0,wc.lpszClassName,L\"SPVersion\",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;")
    set(SP_CREATE_NEW "g_hwnd=CreateWindowExW(0,wc.lpszClassName,L\"SPVersion - Human Combat Demo\",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;RECT client{};GetClientRect(g_hwnd,&client);g_w=client.right-client.left;g_h=client.bottom-client.top;")
    string(REPLACE "${SP_CREATE_OLD}" "${SP_CREATE_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_START_OLD "g_player.pos={0,groundH(0,0),0};g_player.home=g_player.pos;g_npc.pos={8,groundH(8,9),9};g_npc.home=g_npc.pos;g_camTarget={0,1,0};poseCharacter(g_player,\"Idle_A\",0);poseCharacter(g_npc,\"Walking_A\",0);")
    set(SP_START_NEW "resetGame();poseCharacter(g_player,\"Idle_A\",0);poseCharacter(g_npc,\"Running_A\",0);")
    string(REPLACE "${SP_START_OLD}" "${SP_START_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_LOOP_OLD "syncClientSize();updatePlayer(dt);updateNPC(dt);updateCamera(dt);render();Sleep(1);")
    set(SP_LOOP_NEW "syncClientSize();updatePlayer(dt);updateNPC(dt);if(g_gameState==GameState::Playing)resolveCharacterPair(g_player,g_npc);updateCamera(dt);render();Sleep(1);")
    string(REPLACE "${SP_LOOP_OLD}" "${SP_LOOP_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Stop configuration rather than silently shipping a partially patched demo.
    if(SP_DEMO_CODE MATCHES "gltfMatrixRow\\(const float\\* m\\).*m\\[0\\],m\\[4\\],m\\[8\\],m\\[12\\]")
        message(FATAL_ERROR "No se aplico el arreglo de matrices glTF.")
    endif()
    foreach(REQUIRED_TOKEN IN ITEMS resolveCharacterPair GameState::Victory g_attackQueued g_enemyAttackCooldown WM_DPICHANGED SetProcessDpiAwarenessContext resetGame)
        if(NOT SP_DEMO_CODE MATCHES "${REQUIRED_TOKEN}")
            message(FATAL_ERROR "Demo incompleto: falta ${REQUIRED_TOKEN}")
        endif()
    endforeach()

    file(WRITE "${SP_DEMO_GENERATED}" "${SP_DEMO_CODE}")
    add_executable(sp_demo WIN32 "${SP_DEMO_GENERATED}")
    target_include_directories(sp_demo PRIVATE ${CGLTF_DIR})
    target_link_libraries(sp_demo PRIVATE user32 d3d11 dxgi d3dcompiler windowscodecs ole32)
endif()

enable_testing()
add_test(NAME sp_smoke COMMAND sp_smoke)
