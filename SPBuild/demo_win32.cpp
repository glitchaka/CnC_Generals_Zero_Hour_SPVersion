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
static int g_biomass = 250;
static int g_crystals = 100;
static bool g_keys[256] = {};
static LARGE_INTEGER g_freq = {}, g_prev = {};

static COLORREF shade(COLORREF c, float k)
{
    int r=(int)(GetRValue(c)*k), g=(int)(GetGValue(c)*k), b=(int)(GetBValue(c)*k);
    if(r>255)r=255; if(g>255)g=255; if(b>255)b=255;
    return RGB(r,g,b);
}

static COLORREF baseColor(VoxelMaterial m)
{
    switch(m)
    {
        case VOXEL_SOIL: return RGB(116,91,63);
        case VOXEL_GRASS: return RGB(79,137,65);
        case VOXEL_ROCK: return RGB(105,106,99);
        case VOXEL_PEACH_BIOMASS: return RGB(235,157,121);
        case VOXEL_CRYSTAL_CYAN: return RGB(53,224,239);
        case VOXEL_CRYSTAL_MAGENTA: return RGB(226,71,189);
        case VOXEL_CRYSTAL_GREEN: return RGB(110,226,106);
        case VOXEL_WATER: return RGB(65,125,189);
        default: return RGB(38,44,40);
    }
}

static int voxelHeight(VoxelMaterial m)
{
    switch(m)
    {
        case VOXEL_ROCK: return 14;
        case VOXEL_PEACH_BIOMASS: return 10;
        case VOXEL_CRYSTAL_CYAN:
        case VOXEL_CRYSTAL_MAGENTA:
        case VOXEL_CRYSTAL_GREEN: return 30;
        case VOXEL_GRASS: return 5;
        case VOXEL_SOIL: return 4;
        default: return 2;
    }
}

static void polyFill(HDC dc, POINT *p, int n, COLORREF c)
{
    HBRUSH b=CreateSolidBrush(c); HGDIOBJ old=SelectObject(dc,b);
    HPEN pen=CreatePen(PS_SOLID,1,shade(c,0.70f)); HGDIOBJ oldp=SelectObject(dc,pen);
    Polygon(dc,p,n);
    SelectObject(dc,oldp); DeleteObject(pen); SelectObject(dc,old); DeleteObject(b);
}

static POINT iso(int wx,int wy,int z,int ox,int oy)
{
    const int TW=30, TH=15;
    POINT p;
    p.x=ox+(wx-wy)*(TW/2);
    p.y=oy+(wx+wy)*(TH/2)-z;
    return p;
}

static void drawVoxel(HDC dc,int wx,int wy,int z,int h,COLORREF c,int ox,int oy)
{
    POINT t=iso(wx,wy,z+h,ox,oy);
    POINT l=iso(wx,wy+1,z+h,ox,oy);
    POINT b=iso(wx+1,wy+1,z+h,ox,oy);
    POINT r=iso(wx+1,wy,z+h,ox,oy);
    POINT t0=iso(wx,wy,z,ox,oy);
    POINT l0=iso(wx,wy+1,z,ox,oy);
    POINT b0=iso(wx+1,wy+1,z,ox,oy);
    POINT r0=iso(wx+1,wy,z,ox,oy);

    POINT left[4]={l,b,b0,l0};
    POINT right[4]={r,b,b0,r0};
    POINT top[4]={t,r,b,l};
    polyFill(dc,left,4,shade(c,0.72f));
    polyFill(dc,right,4,shade(c,0.86f));
    polyFill(dc,top,4,shade(c,1.08f));
}

static void drawCrystal(HDC dc,int wx,int wy,COLORREF c,int ox,int oy)
{
    drawVoxel(dc,wx,wy,5,28,c,ox,oy);
    POINT tip=iso(wx,wy,42,ox,oy);
    POINT a=iso(wx,wy,33,ox,oy), b=iso(wx+1,wy,33,ox,oy), d=iso(wx,wy+1,33,ox,oy), e=iso(wx+1,wy+1,33,ox,oy);
    POINT f1[3]={tip,a,b}, f2[3]={tip,b,e}, f3[3]={tip,e,d}, f4[3]={tip,d,a};
    polyFill(dc,f1,3,shade(c,1.25f)); polyFill(dc,f2,3,shade(c,0.95f));
    polyFill(dc,f3,3,shade(c,0.78f)); polyFill(dc,f4,3,shade(c,1.10f));
}

static void drawCreature(HDC dc,int ox,int oy)
{
    int x=(int)g_creature.getX(), y=(int)g_creature.getY();
    COLORREF body=RGB(198,81,104);

    drawVoxel(dc,x,y,10,13,body,ox,oy);
    drawVoxel(dc,x+1,y,10,11,shade(body,1.05f),ox,oy);
    drawVoxel(dc,x,y+1,10,9,shade(body,0.95f),ox,oy);
    drawVoxel(dc,x-1,y,8,8,shade(body,0.90f),ox,oy);

    if(g_evo.hasPart(SPORE_PART_EYE))
    {
        drawVoxel(dc,x+1,y-1,19,7,RGB(238,235,210),ox,oy);
        drawVoxel(dc,x+1,y-1,26,3,RGB(28,28,31),ox,oy);
    }
    if(g_evo.hasPart(SPORE_PART_LEG))
    {
        drawVoxel(dc,x-1,y+1,0,12,RGB(151,64,83),ox,oy);
        drawVoxel(dc,x+2,y,0,12,RGB(151,64,83),ox,oy);
    }
    if(g_evo.hasPart(SPORE_PART_MOUTH))
        drawVoxel(dc,x+1,y-1,11,5,RGB(78,27,42),ox,oy);
}

static void addUpgrade(SporePartType type,const char *id,int bio,int cry,int vision,int speed,int damage,int armor)
{
    SporePartUpgrade u; u.type=type; u.id=id; u.biomassCost=bio; u.crystalCost=cry;
    u.visionDelta=vision; u.speedDelta=speed; u.damageDelta=damage; u.armorDelta=armor;
    g_evo.unlock(u,g_biomass,g_crystals);
}

static void initGame()
{
    g_world.resize(80,56); g_world.generate(0xC0FFEEu);
    g_world.addCrystalNode(20,16,7,180,1234u);
    g_world.addCrystalNode(58,35,8,260,777u);
    g_world.addCrystalNode(42,23,5,120,991u);
    g_creature.clear(); g_creature.setPosition(40.0f,28.0f,0.0f); g_evo.resetToVisceroid();
}

static void updateGame(float dt)
{
    float dx=0,dy=0;
    if(g_keys['A']||g_keys[VK_LEFT])dx-=1; if(g_keys['D']||g_keys[VK_RIGHT])dx+=1;
    if(g_keys['W']||g_keys[VK_UP])dy-=1; if(g_keys['S']||g_keys[VK_DOWN])dy+=1;
    float speed=3.0f+(float)g_evo.getSpeed()*0.40f; if(!g_evo.canWalk())speed*=0.46f;
    if(dx||dy)g_creature.move(dx,dy,dt*speed);
    float x=g_creature.getX(), y=g_creature.getY();
    if(x<3)x=3; if(x>76)x=76; if(y<3)y=3; if(y>52)y=52; g_creature.setPosition(x,y,0);
    static unsigned int frame=0; g_world.update(frame++);
}

static void drawTextLine(HDC dc,int x,int y,const std::wstring&s)
{
    SetTextColor(dc,RGB(245,245,245)); SetBkMode(dc,TRANSPARENT); TextOutW(dc,x,y,s.c_str(),(int)s.size());
}

static void renderScene(HDC dc,RECT rc)
{
    HBRUSH bg=CreateSolidBrush(RGB(21,27,25)); FillRect(dc,&rc,bg); DeleteObject(bg);

    int cx=(int)g_creature.getX(), cy=(int)g_creature.getY();
    int ox=(rc.right-rc.left)/2-(cx-cy)*15;
    int oy=165-(cx+cy)*7;

    const int R=24;
    for(int sum=(cx-R)+(cy-R); sum<=(cx+R)+(cy+R); ++sum)
    {
        for(int wx=cx-R; wx<=cx+R; ++wx)
        {
            int wy=sum-wx;
            if(wy<cy-R||wy>cy+R||!g_world.isInside(wx,wy))continue;
            VoxelMaterial m=g_world.getCell(wx,wy).material;
            COLORREF c=baseColor(m);
            if(m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN)
                drawCrystal(dc,wx,wy,c,ox,oy);
            else
                drawVoxel(dc,wx,wy,0,voxelHeight(m),c,ox,oy);
        }
    }

    drawCreature(dc,ox,oy);

    RECT hud={16,16,650,112}; HBRUSH hb=CreateSolidBrush(RGB(17,17,19)); FillRect(dc,&hud,hb); DeleteObject(hb);
    drawTextLine(dc,28,27,L"SPVersion - VOXEL creature prototype");
    drawTextLine(dc,28,49,L"WASD/flechas: mover   H: cosechar   1: ojo   2: piernas   3: boca");
    std::wstring s=L"Biomasa: "+std::to_wstring(g_biomass)+L"   Cristales: "+std::to_wstring(g_crystals)+L"   Vision: "+std::to_wstring(g_evo.getVision())+L"   Velocidad: "+std::to_wstring(g_evo.getSpeed());
    drawTextLine(dc,28,71,s);
    drawTextLine(dc,28,93,g_evo.canWalk()?L"Estado: criatura con locomocion voxel":L"Estado: visceroide voxel rastrero");
}

static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l)
{
    switch(msg)
    {
        case WM_ERASEBKGND: return 1;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_KEYDOWN:
            if(w<256)g_keys[w]=true;
            if(w=='1')addUpgrade(SPORE_PART_EYE,"eye_basic",25,10,4,0,0,0);
            if(w=='2')addUpgrade(SPORE_PART_LEG,"legs_basic",50,20,0,4,0,0);
            if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth_basic",35,15,0,0,3,0);
            if(w=='H') { int got=g_world.harvest((int)g_creature.getX(),(int)g_creature.getY(),8); g_crystals+=got; if(got>0)g_biomass+=got/2; }
            return 0;
        case WM_KEYUP: if(w<256)g_keys[w]=false; return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps; HDC wnd=BeginPaint(h,&ps); RECT rc; GetClientRect(h,&rc);
            HDC mem=CreateCompatibleDC(wnd); HBITMAP bmp=CreateCompatibleBitmap(wnd,rc.right,rc.bottom); HGDIOBJ old=SelectObject(mem,bmp);
            renderScene(mem,rc); BitBlt(wnd,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);
            SelectObject(mem,old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h,&ps); return 0;
        }
    }
    return DefWindowProcW(h,msg,w,l);
}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show)
{
    QueryPerformanceFrequency(&g_freq); QueryPerformanceCounter(&g_prev); initGame();
    WNDCLASSW wc={}; wc.lpfnWndProc=WndProc; wc.hInstance=inst; wc.lpszClassName=L"SPVoxelDemoWindow"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    if(!RegisterClassW(&wc))return 1;
    HWND h=CreateWindowExW(0,wc.lpszClassName,L"SPVersion Voxel Creature Demo",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,820,NULL,NULL,inst,NULL);
    if(!h)return 2; ShowWindow(h,show);

    MSG msg; bool running=true; double accumulator=0.0; const double step=1.0/120.0;
    while(running)
    {
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) { if(msg.message==WM_QUIT){running=false;break;} TranslateMessage(&msg); DispatchMessageW(&msg); }
        LARGE_INTEGER now; QueryPerformanceCounter(&now); double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart; g_prev=now; if(dt>0.05)dt=0.05; accumulator+=dt;
        while(accumulator>=step){updateGame((float)step); accumulator-=step;}
        InvalidateRect(h,NULL,FALSE); UpdateWindow(h); Sleep(1);
    }
    return 0;
}
