#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <string>
#include "SporePrototype/SporeVoxelWorld.h"
#include "SporePrototype/SporeCreature.h"
#include "Spore/SporeEvolution.h"

using namespace SporePrototype;

static SporeVoxelWorld g_world;
static SporeCreature g_creature;
static SporeEvolutionState g_evo;
static int g_biomass = 250, g_crystals = 100;
static bool g_keys[256] = {};
static LARGE_INTEGER g_freq = {}, g_prev = {};
static float g_anim = 0.0f;
static HWND g_hwnd = NULL;
static HDC g_backDC = NULL;
static HBITMAP g_backBmp = NULL, g_backOld = NULL;
static int g_backW = 0, g_backH = 0;

static COLORREF shade(COLORREF c, float k) {
    int r=(int)(GetRValue(c)*k), g=(int)(GetGValue(c)*k), b=(int)(GetBValue(c)*k);
    if(r<0)r=0; if(g<0)g=0; if(b<0)b=0; if(r>255)r=255; if(g>255)g=255; if(b>255)b=255;
    return RGB(r,g,b);
}
static COLORREF matColor(VoxelMaterial m) {
    switch(m) {
        case VOXEL_SOIL: return RGB(105,86,64);
        case VOXEL_GRASS: return RGB(66,124,57);
        case VOXEL_ROCK: return RGB(95,97,94);
        case VOXEL_PEACH_BIOMASS: return RGB(228,149,116);
        case VOXEL_CRYSTAL_CYAN: return RGB(45,219,235);
        case VOXEL_CRYSTAL_MAGENTA: return RGB(218,67,179);
        case VOXEL_CRYSTAL_GREEN: return RGB(105,219,102);
        case VOXEL_WATER: return RGB(55,110,171);
        default: return RGB(35,41,37);
    }
}
static int matHeight(VoxelMaterial m) {
    switch(m) {
        case VOXEL_ROCK: return 4;
        case VOXEL_PEACH_BIOMASS: return 3;
        case VOXEL_GRASS: return 2;
        case VOXEL_SOIL: return 2;
        default: return 1;
    }
}

static void setBrush(HDC dc, COLORREF c){ SelectObject(dc,GetStockObject(DC_BRUSH)); SetDCBrushColor(dc,c); }
static void setPen(HDC dc, COLORREF c){ SelectObject(dc,GetStockObject(DC_PEN)); SetDCPenColor(dc,c); }
static void poly(HDC dc, POINT *p, int n, COLORREF c){ setBrush(dc,c); setPen(dc,shade(c,.67f)); Polygon(dc,p,n); }
static void fill(HDC dc, const RECT &r, COLORREF c){ setBrush(dc,c); FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH)); }

static void destroyBackBuffer(){
    if(g_backDC){
        if(g_backOld) SelectObject(g_backDC,g_backOld);
        if(g_backBmp) DeleteObject(g_backBmp);
        DeleteDC(g_backDC);
    }
    g_backDC=NULL; g_backBmp=NULL; g_backOld=NULL; g_backW=g_backH=0;
}
static void ensureBackBuffer(HWND h,int w,int ht){
    if(w<=0||ht<=0) return;
    if(g_backDC && w==g_backW && ht==g_backH) return;
    destroyBackBuffer();
    HDC wnd=GetDC(h);
    g_backDC=CreateCompatibleDC(wnd);
    g_backBmp=CreateCompatibleBitmap(wnd,w,ht);
    g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);
    ReleaseDC(h,wnd);
    g_backW=w; g_backH=ht;
}

// World voxels are intentionally tiny: each tile is only 10x5 screen pixels.
static POINT iso(float wx,float wy,int z,float camX,float camY,int ox,int oy){
    const float TW=10.0f, TH=5.0f;
    float rx=wx-camX, ry=wy-camY;
    POINT p={(LONG)lroundf(ox+(rx-ry)*(TW*0.5f)),(LONG)lroundf(oy+(rx+ry)*(TH*0.5f)-z)};
    return p;
}
static void worldVoxel(HDC dc,float wx,float wy,int z,int h,COLORREF c,float camX,float camY,int ox,int oy){
    POINT t=iso(wx,wy,z+h,camX,camY,ox,oy), r=iso(wx+1,wy,z+h,camX,camY,ox,oy);
    POINT b=iso(wx+1,wy+1,z+h,camX,camY,ox,oy), l=iso(wx,wy+1,z+h,camX,camY,ox,oy);
    POINT r0=iso(wx+1,wy,z,camX,camY,ox,oy), b0=iso(wx+1,wy+1,z,camX,camY,ox,oy), l0=iso(wx,wy+1,z,camX,camY,ox,oy);
    POINT left[4]={l,b,b0,l0}, right[4]={r,b,b0,r0}, top[4]={t,r,b,l};
    poly(dc,left,4,shade(c,.72f)); poly(dc,right,4,shade(c,.86f)); poly(dc,top,4,shade(c,1.08f));
}
static void microCube(HDC dc,int x,int y,int z,COLORREF c){
    // one visual voxel ~= 4 pixels wide, like a 3D pixel instead of a block
    int hw=3, hh=2, vh=3;
    POINT top[4]={{x,y-z-hh},{x+hw,y-z},{x,y-z+hh},{x-hw,y-z}};
    POINT left[4]={{x-hw,y-z},{x,y-z+hh},{x,y-z+hh+vh},{x-hw,y-z+vh}};
    POINT right[4]={{x+hw,y-z},{x,y-z+hh},{x,y-z+hh+vh},{x+hw,y-z+vh}};
    poly(dc,left,4,shade(c,.72f)); poly(dc,right,4,shade(c,.86f)); poly(dc,top,4,shade(c,1.08f));
}

static bool isCrystal(VoxelMaterial m){
    return m==VOXEL_CRYSTAL_CYAN || m==VOXEL_CRYSTAL_MAGENTA || m==VOXEL_CRYSTAL_GREEN;
}
static void crystalCluster(HDC dc,int wx,int wy,COLORREF c,float camX,float camY,int ox,int oy){
    POINT p=iso((float)wx+.5f,(float)wy+.5f,3,camX,camY,ox,oy);
    for(int z=0;z<7;++z) microCube(dc,p.x,p.y-z*2,z*2,shade(c,1.0f+0.025f*z));
    microCube(dc,p.x-5,p.y+3,0,shade(c,.88f));
    microCube(dc,p.x+5,p.y+3,0,shade(c,1.10f));
    microCube(dc,p.x+2,p.y-3,2,shade(c,.96f));
}

static bool insideLobe(float x,float y,float cx,float cy,float rx,float ry){
    float dx=(x-cx)/rx, dy=(y-cy)/ry;
    return dx*dx+dy*dy <= 1.0f;
}
static bool visceroidShape(int x,int y,int z){
    float shrink=1.0f-(float)z*0.10f;
    if(shrink<.48f) shrink=.48f;
    // three broad arms around a central knot, matching the classic Visceroid silhouette
    bool a=insideLobe((float)x,(float)y, 0,-6, 5.5f*shrink,10.5f*shrink);
    bool b=insideLobe((float)x,(float)y,-7, 4,10.5f*shrink, 5.5f*shrink);
    bool c=insideLobe((float)x,(float)y, 7, 4,10.5f*shrink, 5.5f*shrink);
    bool core=insideLobe((float)x,(float)y,0,1,7.0f*shrink,6.0f*shrink);
    return a||b||c||core;
}
static void drawVisceroid(HDC dc,int cx,int cy){
    const COLORREF flesh=RGB(176,78,80), light=RGB(205,112,104), dark=RGB(115,47,55);
    int bob=(int)lroundf(sinf(g_anim)*2.0f);
    for(int z=0;z<6;++z){
        for(int y=-12;y<=10;++y){
            for(int x=-13;x<=13;++x){
                if(!visceroidShape(x,y,z)) continue;
                if(((x*13+y*7+z*5)&3)==0 && z>2) continue; // pores / irregular organic surface
                COLORREF c=((x+y+z)&1)?flesh:light;
                int sx=cx + (x-y)*3;
                int sy=cy + (x+y) + bob - z*3;
                microCube(dc,sx,sy,z,c);
            }
        }
    }
    // dark central fissure gives it the recognisable organic Visceroid read
    for(int i=-4;i<=4;++i) microCube(dc,cx+i*2,cy-10+abs(i),8,dark);

    if(g_evo.hasPart(SPORE_PART_EYE)){
        microCube(dc,cx,cy-28+bob,10,RGB(225,230,210));
        microCube(dc,cx,cy-31+bob,12,RGB(27,28,31));
    }
    if(g_evo.hasPart(SPORE_PART_LEG)){
        for(int i=0;i<5;++i){ microCube(dc,cx-34,cy+18+i*3,0,dark); microCube(dc,cx+34,cy+18+i*3,0,dark); }
    }
    if(g_evo.hasPart(SPORE_PART_MOUTH)){
        for(int i=-4;i<=4;++i) microCube(dc,cx+i*3,cy+10+abs(i),5,RGB(69,25,34));
    }
}

static void addUpgrade(SporePartType type,const char *id,int bio,int cry,int vis,int speed,int dmg,int armor){
    SporePartUpgrade u; u.type=type; u.id=id; u.biomassCost=bio; u.crystalCost=cry;
    u.visionDelta=vis; u.speedDelta=speed; u.damageDelta=dmg; u.armorDelta=armor;
    g_evo.unlock(u,g_biomass,g_crystals);
}
static void initGame(){
    g_world.resize(80,56); g_world.generate(0xC0FFEEu);
    g_world.addCrystalNode(36,24,5,160,1234u); g_world.addCrystalNode(47,31,6,220,777u); g_world.addCrystalNode(28,34,5,120,991u);
    g_creature.clear(); g_creature.setPosition(40.0f,28.0f,0.0f); g_evo.resetToVisceroid();
}
static void updateGame(float dt){
    float dx=0,dy=0;
    if(g_keys['A']||g_keys[VK_LEFT]) dx-=1; if(g_keys['D']||g_keys[VK_RIGHT]) dx+=1;
    if(g_keys['W']||g_keys[VK_UP]) dy-=1; if(g_keys['S']||g_keys[VK_DOWN]) dy+=1;
    if(dx||dy){
        float len=sqrtf(dx*dx+dy*dy); dx/=len; dy/=len;
        float speed=6.5f+(float)g_evo.getSpeed()*.45f; if(!g_evo.canWalk()) speed*=.60f;
        g_creature.move(dx,dy,dt*speed);
    }
    float x=g_creature.getX(), y=g_creature.getY();
    if(x<2)x=2; if(x>77)x=77; if(y<2)y=2; if(y>53)y=53;
    g_creature.setPosition(x,y,0); g_anim+=dt*6.0f;
    static unsigned int frame=0; g_world.update(frame++);
}
static void text(HDC dc,int x,int y,const std::wstring&s){ SetTextColor(dc,RGB(242,242,242)); SetBkMode(dc,TRANSPARENT); TextOutW(dc,x,y,s.c_str(),(int)s.size()); }

static void renderScene(HDC dc,RECT rc){
    fill(dc,rc,RGB(23,29,27));
    const int ox=(rc.right-rc.left)/2, oy=(rc.bottom-rc.top)*56/100;
    const float camX=g_creature.getX(), camY=g_creature.getY();

    // Painter order across the complete 80x56 map. Projection uses FLOAT camera coords,
    // so even a tiny key press produces immediate on-screen motion.
    for(int sum=0;sum<80+56;++sum){
        for(int wx=0;wx<80;++wx){
            int wy=sum-wx; if(wy<0||wy>=56) continue;
            POINT p=iso((float)wx,(float)wy,0,camX,camY,ox,oy);
            if(p.x<-20||p.x>rc.right+20||p.y<100||p.y>rc.bottom+20) continue;
            VoxelMaterial m=g_world.getCell(wx,wy).material;
            if(isCrystal(m)){
                worldVoxel(dc,(float)wx,(float)wy,0,2,RGB(222,148,112),camX,camY,ox,oy);
                crystalCluster(dc,wx,wy,matColor(m),camX,camY,ox,oy);
            } else {
                worldVoxel(dc,(float)wx,(float)wy,0,matHeight(m),matColor(m),camX,camY,ox,oy);
            }
        }
    }

    drawVisceroid(dc,ox,oy-18);

    RECT hud={16,16,760,116}; fill(dc,hud,RGB(14,15,17));
    text(dc,28,26,L"SPVersion - microvoxel Visceroid evolution demo");
    text(dc,28,48,L"WASD/flechas: mover   H: cosechar   1: ojo   2: piernas   3: boca");
    std::wstring s=L"Biomasa: "+std::to_wstring(g_biomass)+L"   Cristales: "+std::to_wstring(g_crystals)+L"   Vision: "+std::to_wstring(g_evo.getVision())+L"   Velocidad: "+std::to_wstring(g_evo.getSpeed());
    text(dc,28,70,s); text(dc,28,92,g_evo.canWalk()?L"Estado: locomocion evolucionada":L"Estado inicial: Visceroid rastrero");
}
static void renderFrame(HWND h){
    RECT rc; GetClientRect(h,&rc); int w=rc.right, ht=rc.bottom; if(w<=0||ht<=0)return;
    ensureBackBuffer(h,w,ht); renderScene(g_backDC,rc);
    HDC wnd=GetDC(h); BitBlt(wnd,0,0,w,ht,g_backDC,0,0,SRCCOPY); ReleaseDC(h,wnd);
}

static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
        case WM_ERASEBKGND:return 1;
        case WM_SIZE:destroyBackBuffer();return 0;
        case WM_DESTROY:destroyBackBuffer();PostQuitMessage(0);return 0;
        case WM_KEYDOWN:
            if(w<256)g_keys[w]=true;
            if(w=='1')addUpgrade(SPORE_PART_EYE,"eye_basic",25,10,4,0,0,0);
            if(w=='2')addUpgrade(SPORE_PART_LEG,"legs_basic",50,20,0,4,0,0);
            if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth_basic",35,15,0,0,3,0);
            if(w=='H'){int got=g_world.harvest((int)g_creature.getX(),(int)g_creature.getY(),8);g_crystals+=got;if(got>0)g_biomass+=got/2;}
            return 0;
        case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}
    }
    return DefWindowProcW(h,msg,w,l);
}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){
    QueryPerformanceFrequency(&g_freq); QueryPerformanceCounter(&g_prev); initGame();
    WNDCLASSW wc={}; wc.lpfnWndProc=WndProc; wc.hInstance=inst; wc.lpszClassName=L"SPMicroVoxelWindow"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    if(!RegisterClassW(&wc)) return 1;
    g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion - Microvoxel Visceroid Demo",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,820,NULL,NULL,inst,NULL);
    if(!g_hwnd)return 2; ShowWindow(g_hwnd,show);
    MSG msg; bool running=true; double logicAcc=0,renderAcc=0; const double logicStep=1.0/120.0, renderStep=1.0/60.0;
    while(running){
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){ if(msg.message==WM_QUIT){running=false;break;} TranslateMessage(&msg);DispatchMessageW(&msg); }
        LARGE_INTEGER now; QueryPerformanceCounter(&now); double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart; g_prev=now; if(dt>.05)dt=.05;
        logicAcc+=dt; renderAcc+=dt;
        while(logicAcc>=logicStep){updateGame((float)logicStep);logicAcc-=logicStep;}
        if(renderAcc>=renderStep){renderFrame(g_hwnd);renderAcc=fmod(renderAcc,renderStep);}
        Sleep(1);
    }
    destroyBackBuffer(); return 0;
}
