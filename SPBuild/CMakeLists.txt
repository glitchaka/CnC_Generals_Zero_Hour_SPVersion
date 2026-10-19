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

    # Generate the Windows demo from the checked-in source while keeping the
    # fixes local to this small SP prototype. This avoids touching the original
    # Generals codebase and makes the expected human-controller behavior explicit.
    set(SP_DEMO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/demo_d3d11_human.cpp")
    set(SP_DEMO_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/demo_d3d11_human_fixed.cpp")
    file(READ "${SP_DEMO_SOURCE}" SP_DEMO_CODE)

    # glTF matrices are column-major/column-vector; DirectXMath code below uses
    # row vectors. Keep translation in the correct row so skinned vertices do
    # not acquire a bogus W and shoot to infinity.
    set(SP_GLTF_MATRIX_BROKEN "static XMMATRIX gltfMatrixRow(const float* m){ return XMMATRIX(m[0],m[4],m[8],m[12], m[1],m[5],m[9],m[13], m[2],m[6],m[10],m[14], m[3],m[7],m[11],m[15]); }")
    set(SP_GLTF_MATRIX_FIXED  "static XMMATRIX gltfMatrixRow(const float* m){ return XMMATRIX(m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7], m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15]); }")
    string(REPLACE "${SP_GLTF_MATRIX_BROKEN}" "${SP_GLTF_MATRIX_FIXED}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # The old prototype only resolved characters against scenery, so player and
    # NPC could pass straight through one another. Add a symmetric capsule-like
    # ground-plane separation and resolve scenery again after the push.
    set(SP_RESOLVE_OLD [=[static void resolveCharacter(Character& c){
    const float cr=.42f; for(const auto& o:g_scene){ if(!o.collider||!o.mesh)continue; float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.42f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.42f,r=(std::max)(.55f,(std::max)(rx,rz)); float dx=c.pos.x-o.x,dz=c.pos.z-o.z,ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr); if(ds<rr&&ds>1e-6f){float d=sqrtf(ds),push=r+cr-d;c.pos.x+=dx/d*push;c.pos.z+=dz/d*push;} } c.pos.y=groundH(c.pos.x,c.pos.z);
}]=])
    set(SP_RESOLVE_NEW [=[static void resolveCharacter(Character& c){
    const float cr=.42f; for(const auto& o:g_scene){ if(!o.collider||!o.mesh)continue; float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.42f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.42f,r=(std::max)(.55f,(std::max)(rx,rz)); float dx=c.pos.x-o.x,dz=c.pos.z-o.z,ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr); if(ds<rr&&ds>1e-6f){float d=sqrtf(ds),push=r+cr-d;c.pos.x+=dx/d*push;c.pos.z+=dz/d*push;} } c.pos.y=groundH(c.pos.x,c.pos.z);
}

static void resolveCharacterPair(Character& a,Character& b){
    const float ra=.48f,rb=.48f,minD=ra+rb; float dx=a.pos.x-b.pos.x,dz=a.pos.z-b.pos.z,ds=dx*dx+dz*dz;
    if(ds>=minD*minD)return;
    if(ds<1e-8f){dx=1.f;dz=0.f;ds=1.f;}
    float d=sqrtf(ds),push=(minD-d)*.5f,nx=dx/d,nz=dz/d;
    a.pos.x+=nx*push;a.pos.z+=nz*push;b.pos.x-=nx*push;b.pos.z-=nz*push;
    resolveCharacter(a);resolveCharacter(b);
}]=])
    string(REPLACE "${SP_RESOLVE_OLD}" "${SP_RESOLVE_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Keep WASD relative to camera, but give the controller a normal walk/run
    # split: movement is walk by default and Shift runs. The character still
    # turns toward the actual movement vector.
    set(SP_PLAYER_OLD [=[static void updatePlayer(float dt){
    float side=(g_keys['D']?1.f:0.f)-(g_keys['A']?1.f:0.f),fwd=(g_keys['W']?1.f:0.f)-(g_keys['S']?1.f:0.f); float m=sqrtf(side*side+fwd*fwd); if(m>1){side/=m;fwd/=m;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd; float dm=sqrtf(dx*dx+dz*dz); g_player.moving=dm>.01f; g_player.speed=g_player.moving?4.8f:0;
    if(g_player.moving){dx/=dm;dz/=dm;g_player.yaw=approachAngle(g_player.yaw,atan2f(dx,dz),10.f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;}
    resolveCharacter(g_player); poseCharacter(g_player,g_player.moving?"Running_A":"Idle_A",dt,g_player.moving?1.05f:1.f);
}]=])
    set(SP_PLAYER_NEW [=[static void updatePlayer(float dt){
    float side=(g_keys['D']?1.f:0.f)-(g_keys['A']?1.f:0.f),fwd=(g_keys['W']?1.f:0.f)-(g_keys['S']?1.f:0.f); float m=sqrtf(side*side+fwd*fwd); if(m>1){side/=m;fwd/=m;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd; float dm=sqrtf(dx*dx+dz*dz); bool run=g_keys[VK_SHIFT]; g_player.moving=dm>.01f; g_player.speed=g_player.moving?(run?5.4f:3.2f):0.f;
    if(g_player.moving){dx/=dm;dz/=dm;g_player.yaw=approachAngle(g_player.yaw,atan2f(dx,dz),12.f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;}
    resolveCharacter(g_player); poseCharacter(g_player,g_player.moving?(run?"Running_A":"Walking_A"):"Idle_A",dt,g_player.moving?(run?1.05f:1.f):1.f);
}]=])
    string(REPLACE "${SP_PLAYER_OLD}" "${SP_PLAYER_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # DPI virtualization was making the physical window larger than the D3D
    # backbuffer on scaled Windows desktops, which is exactly the black area
    # captured by Win+G. Handle DPI before window creation and resize on changes.
    set(SP_WMSIZE_OLD "        case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;")
    set(SP_WMSIZE_NEW [=[        case WM_SIZE:if(g_swap&&w!=SIZE_MINIMIZED)resizeSwap(LOWORD(l),HIWORD(l));return 0;
        case WM_DPICHANGED:{RECT* r=(RECT*)l;SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}]=])
    string(REPLACE "${SP_WMSIZE_OLD}" "${SP_WMSIZE_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_MAIN_OLD "int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){\n    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);")
    set(SP_MAIN_NEW [=[int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,PWSTR,int){
    using SetDpiCtxFn=BOOL (WINAPI*)(HANDLE); HMODULE u32=GetModuleHandleW(L"user32.dll"); SetDpiCtxFn setDpi=(SetDpiCtxFn)GetProcAddress(u32,"SetProcessDpiAwarenessContext"); if(setDpi)setDpi((HANDLE)-4);else SetProcessDPIAware();
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);]=])
    string(REPLACE "${SP_MAIN_OLD}" "${SP_MAIN_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_CREATE_OLD "g_hwnd=CreateWindowExW(0,wc.lpszClassName,L\"SPVersion\",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;")
    set(SP_CREATE_NEW "g_hwnd=CreateWindowExW(0,wc.lpszClassName,L\"SPVersion\",WS_POPUP|WS_VISIBLE,x,y,g_w,g_h,nullptr,nullptr,hi,nullptr);if(!g_hwnd)return 1;RECT client{};GetClientRect(g_hwnd,&client);g_w=client.right-client.left;g_h=client.bottom-client.top;")
    string(REPLACE "${SP_CREATE_OLD}" "${SP_CREATE_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    set(SP_LOOP_OLD "syncClientSize();updatePlayer(dt);updateNPC(dt);updateCamera(dt);render();Sleep(1);")
    set(SP_LOOP_NEW "syncClientSize();updatePlayer(dt);updateNPC(dt);resolveCharacterPair(g_player,g_npc);updateCamera(dt);render();Sleep(1);")
    string(REPLACE "${SP_LOOP_OLD}" "${SP_LOOP_NEW}" SP_DEMO_CODE "${SP_DEMO_CODE}")

    # Refuse to build a silently unmodified demo if any expected source anchor
    # disappeared in a later edit.
    if(SP_DEMO_CODE MATCHES "gltfMatrixRow\\(const float\\* m\\).*m\\[0\\],m\\[4\\],m\\[8\\],m\\[12\\]")
        message(FATAL_ERROR "No se aplico el arreglo de matrices glTF.")
    endif()
    if(NOT SP_DEMO_CODE MATCHES "resolveCharacterPair")
        message(FATAL_ERROR "No se aplico la colision jugador-NPC.")
    endif()
    if(NOT SP_DEMO_CODE MATCHES "SetProcessDpiAwarenessContext")
        message(FATAL_ERROR "No se aplico el arreglo DPI de la ventana.")
    endif()

    file(WRITE "${SP_DEMO_GENERATED}" "${SP_DEMO_CODE}")

    add_executable(sp_demo WIN32 "${SP_DEMO_GENERATED}")
    target_include_directories(sp_demo PRIVATE ${CGLTF_DIR})
    target_link_libraries(sp_demo PRIVATE user32 d3d11 dxgi d3dcompiler windowscodecs ole32)
endif()

enable_testing()
add_test(NAME sp_smoke COMMAND sp_smoke)
