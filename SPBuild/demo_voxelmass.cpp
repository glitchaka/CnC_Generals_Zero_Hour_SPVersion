#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include "SporePrototype/SporeVoxelWorld.h"
#include "SporePrototype/SporeCreature.h"
#include "Spore/SporeEvolution.h"

using namespace SporePrototype;

static SporeVoxelWorld g_world;
static SporeCreature g_creature;
static SporeEvolutionState g_evo;
static int g_biomass=250, g_crystals=100;
static bool g_keys[256]={};
static LARGE_INTEGER g_freq={}, g_prev={};
static float g_anim=0.0f, g_moveX=0.0f, g_moveY=0.0f;
static float g_camX=40.0f,g_camY=28.0f;
static HWND g_hwnd=NULL;
static HDC g_backDC=NULL;
static HBITMAP g_backBmp=NULL;
static HBITMAP g_backOld=NULL;
static uint32_t* g_pixels=nullptr;
static int g_backW=0,g_backH=0;

static COLORREF shade(COLORREF c,float k){
    int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);
    r=(std::max)(0,(std::min)(255,r)); g=(std::max)(0,(std::min)(255,g)); b=(std::max)(0,(std::min)(255,b));
    return RGB(r,g,b);
}
static COLORREF baseColor(VoxelMaterial m){
    switch(m){
        case VOXEL_SOIL:return RGB(96,77,53);
        case VOXEL_GRASS:return RGB(72,117,59);
        case VOXEL_ROCK:return RGB(93,96,92);
        case VOXEL_PEACH_BIOMASS:return RGB(221,145,112);
        case VOXEL_CRYSTAL_CYAN:return RGB(47,211,224);
        case VOXEL_CRYSTAL_MAGENTA:return RGB(198,57,164);
        case VOXEL_CRYSTAL_GREEN:return RGB(94,201,96);
        case VOXEL_WATER:return RGB(56,108,159);
        default:return RGB(35,41,38);
    }
}
static void setBrush(HDC dc,COLORREF c){SelectObject(dc,GetStockObject(DC_BRUSH));SetDCBrushColor(dc,c);}
static void setPen(HDC dc,COLORREF c){SelectObject(dc,GetStockObject(DC_PEN));SetDCPenColor(dc,c);}
static void polyFill(HDC dc,POINT*p,int n,COLORREF c){setBrush(dc,c);setPen(dc,shade(c,.78f));Polygon(dc,p,n);}
static void fillSolid(HDC dc,const RECT&r,COLORREF c){setBrush(dc,c);FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH));}

static void destroyBackBuffer(){
    g_pixels=nullptr;
    if(g_backDC){
        if(g_backOld)SelectObject(g_backDC,g_backOld);
        g_backOld=NULL;
        if(g_backBmp)DeleteObject(g_backBmp);
        g_backBmp=NULL;
        DeleteDC(g_backDC);
        g_backDC=NULL;
    }
    g_backW=g_backH=0;
}
static void ensureBackBuffer(HWND h,int w,int ht){
    if(w<=0||ht<=0)return;
    if(g_backDC&&w==g_backW&&ht==g_backH)return;
    destroyBackBuffer();
    HDC wnd=GetDC(h);
    g_backDC=CreateCompatibleDC(wnd);
    BITMAPINFO bi={};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=w;
    bi.bmiHeader.biHeight=-ht;
    bi.bmiHeader.biPlanes=1;
    bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    g_backBmp=CreateDIBSection(wnd,&bi,DIB_RGB_COLORS,&bits,NULL,0);
    g_pixels=(uint32_t*)bits;
    g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);
    ReleaseDC(h,wnd);
    g_backW=w;g_backH=ht;
    SelectObject(g_backDC,GetStockObject(DC_BRUSH));
    SelectObject(g_backDC,GetStockObject(DC_PEN));
}
static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|(uint32_t)GetBValue(c);}
static inline void px(int x,int y,COLORREF c){if((unsigned)x>=(unsigned)g_backW||(unsigned)y>=(unsigned)g_backH||!g_pixels)return;g_pixels[y*g_backW+x]=pack(c);}
static inline void splat(int x,int y,COLORREF c){px(x,y,c);px(x+1,y,c);px(x,y+1,c);px(x+1,y+1,c);}

static POINT isoWorld(float wx,float wy,float z,int centerX,int centerY){
    const float TW=40.0f,TH=20.0f;
    float rx=wx-g_camX,ry=wy-g_camY;
    POINT p={(LONG)lroundf(centerX+(rx-ry)*(TW*.5f)),(LONG)lroundf(centerY+(rx+ry)*(TH*.5f)-z)};
    return p;
}
static void drawGroundTile(HDC dc,int wx,int wy,COLORREF c,int centerX,int centerY){
    POINT a=isoWorld((float)wx,(float)wy,0,centerX,centerY),b=isoWorld((float)wx+1,(float)wy,0,centerX,centerY),d=isoWorld((float)wx,(float)wy+1,0,centerX,centerY),e=isoWorld((float)wx+1,(float)wy+1,0,centerX,centerY);
    POINT q[4]={a,b,e,d}; polyFill(dc,q,4,c);
}

static bool ellipsoid(float x,float y,float z,float cx,float cy,float cz,float rx,float ry,float rz){
    float dx=(x-cx)/rx,dy=(y-cy)/ry,dz=(z-cz)/rz;return dx*dx+dy*dy+dz*dz<=1.0f;
}
static bool bodyAt(int x,int y,int z){
    if(z<0||z>9)return false;
    float crawl=(fabsf(g_moveX)+fabsf(g_moveY))>0.05f?1.0f:0.35f;
    float a=g_anim*crawl;
    float sway=sinf(a)*1.15f;
    float squeeze=cosf(a*1.35f)*.7f;
    bool b=false;
    b|=ellipsoid((float)x,(float)y,(float)z,-5.8f+sway,-1.5f,2.7f,7.7f+squeeze,5.6f,3.2f);
    b|=ellipsoid((float)x,(float)y,(float)z, 5.8f-sway,-1.5f,2.7f,7.7f-squeeze*.4f,5.6f,3.2f);
    b|=ellipsoid((float)x,(float)y,(float)z, 0.0f, 5.5f+sinf(a*.8f)*.8f,2.2f,6.4f,8.4f,2.8f);
    b|=ellipsoid((float)x,(float)y,(float)z, 0.0f, 1.0f,2.5f,8.5f,5.8f,3.3f);
    bool cleft=ellipsoid((float)x,(float)y,(float)z,0.0f,-5.7f,4.7f,2.0f,5.2f,4.0f);
    return b&&!cleft;
}
struct V{int x,y,z;COLORREF c;};
static void buildBody(std::vector<V>& out){
    out.clear();out.reserve(5000);
    for(int y=-12;y<=13;++y)for(int x=-15;x<=15;++x)for(int z=0;z<=9;++z){
        if(!bodyAt(x,y,z))continue;
        unsigned h=(unsigned)(x*73856093u)^(unsigned)(y*19349663u)^(unsigned)(z*83492791u);
        float hi=.86f+.035f*(float)z;
        COLORREF base=(h&7u)<2?RGB(111,43,49):((h&7u)<5?RGB(137,51,57):RGB(160,61,66));
        out.push_back({x,y,z,shade(base,hi)});
    }
    if(g_evo.hasPart(SPORE_PART_EYE)){
        for(int y=-7;y<=-3;++y)for(int x=3;x<=7;++x)for(int z=3;z<=7;++z)
            if(ellipsoid((float)x,(float)y,(float)z,5.0f,-4.7f,5.0f,2.5f,2.2f,2.2f))out.push_back({x,y,z,RGB(205,213,191)});
        out.push_back({5,-7,5,RGB(18,24,23)});out.push_back({5,-7,6,RGB(18,24,23)});
    }
    if(g_evo.hasPart(SPORE_PART_LEG)){
        for(int z=0;z<6;++z){out.push_back({-10,6,z,RGB(101,38,44)});out.push_back({10,6,z,RGB(101,38,44)});out.push_back({-6,10,z,RGB(101,38,44)});out.push_back({6,10,z,RGB(101,38,44)});}
    }
    if(g_evo.hasPart(SPORE_PART_MOUTH))for(int x=-3;x<=3;++x){out.push_back({x,-9,2,RGB(55,18,22)});out.push_back({x,-9,3,RGB(67,21,26)});}
    std::sort(out.begin(),out.end(),[](const V&a,const V&b){int da=a.x+a.y+a.z*2,db=b.x+b.y+b.z*2;return da<db;});
}
static void drawCreature(HDC dc,int centerX,int centerY){
    POINT a=isoWorld(g_creature.getX(),g_creature.getY(),0,centerX,centerY);
    setBrush(dc,RGB(28,33,30));setPen(dc,RGB(28,33,30));
    Ellipse(dc,a.x-42,a.y-10,a.x+42,a.y+12);
    std::vector<V> vox;buildBody(vox);
    for(const V&v:vox){
        int sx=a.x+(v.x-v.y)*2;
        int sy=a.y+(v.x+v.y)-v.z*2;
        splat(sx,sy,v.c);
    }
}
static void drawCrystalCluster(int wx,int wy,COLORREF c,int centerX,int centerY){
    POINT a=isoWorld((float)wx+.5f,(float)wy+.5f,0,centerX,centerY);
    const int stems[4][3]={{-5,2,12},{2,1,16},{7,4,10},{-1,-3,9}};
    for(int s=0;s<4;++s){
        for(int z=0;z<stems[s][2];++z){
            int sx=a.x+stems[s][0]+((z&1)?1:0);
            int sy=a.y+stems[s][1]-z*2;
            COLORREF cc=shade(c,.82f+.025f*(float)z);
            splat(sx,sy,cc);
            if(z<stems[s][2]-2)splat(sx+2,sy,shade(cc,.86f));
        }
    }
}

static void addUpgrade(SporePartType type,const char*id,int bio,int cry,int vision,int speed,int damage,int armor){SporePartUpgrade u;u.type=type;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vision;u.speedDelta=speed;u.damageDelta=damage;u.armorDelta=armor;g_evo.unlock(u,g_biomass,g_crystals);}
static void initGame(){g_world.resize(80,56);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(36,24,6,180,1234u);g_world.addCrystalNode(47,31,7,260,777u);g_world.addCrystalNode(28,34,5,120,991u);g_creature.clear();g_creature.setPosition(40.0f,28.0f,0.0f);g_camX=40.0f;g_camY=28.0f;g_evo.resetToVisceroid();}
static void updateGame(float dt){
    float dx=0,dy=0;if(g_keys['A']||g_keys[VK_LEFT])dx-=1;if(g_keys['D']||g_keys[VK_RIGHT])dx+=1;if(g_keys['W']||g_keys[VK_UP])dy-=1;if(g_keys['S']||g_keys[VK_DOWN])dy+=1;
    if(dx&&dy){dx*=.7071067f;dy*=.7071067f;}g_moveX=dx;g_moveY=dy;
    float speed=4.7f+(float)g_evo.getSpeed()*.48f;if(!g_evo.canWalk())speed*=.62f;
    if(dx||dy)g_creature.move(dx,dy,dt*speed);
    float x=(std::max)(3.0f,(std::min)(76.0f,g_creature.getX()));float y=(std::max)(3.0f,(std::min)(52.0f,g_creature.getY()));g_creature.setPosition(x,y,0);
    float follow=1.0f-expf(-dt*7.5f);g_camX+=(x-g_camX)*follow;g_camY+=(y-g_camY)*follow;
    g_anim+=dt*((dx||dy)?9.0f:2.2f);static unsigned int frame=0;g_world.update(frame++);
}
static void text(HDC dc,int x,int y,const std::wstring&s){SetTextColor(dc,RGB(238,238,238));SetBkMode(dc,TRANSPARENT);TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void renderScene(HDC dc,RECT rc){
    fillSolid(dc,rc,RGB(24,29,27));int centerX=(rc.right-rc.left)/2,centerY=(rc.bottom-rc.top)*57/100;const int R=15;
    int minX=(int)floorf(g_camX)-R,maxX=(int)ceilf(g_camX)+R,minY=(int)floorf(g_camY)-R,maxY=(int)ceilf(g_camY)+R;
    for(int sum=minX+minY;sum<=maxX+maxY;++sum)for(int wx=minX;wx<=maxX;++wx){
        int wy=sum-wx;if(wy<minY||wy>maxY||!g_world.isInside(wx,wy))continue;
        VoxelMaterial m=g_world.getCell(wx,wy).material;COLORREF c=baseColor(m);
        bool crystal=m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;
        drawGroundTile(dc,wx,wy,crystal?RGB(78,112,62):c,centerX,centerY);
        if(crystal)drawCrystalCluster(wx,wy,c,centerX,centerY);
    }
    drawCreature(dc,centerX,centerY);
    RECT hud={16,16,760,116};fillSolid(dc,hud,RGB(12,14,14));
    text(dc,28,26,L"SPVersion - Visceroid voxel-mass prototype");
    text(dc,28,49,L"WASD/flechas: mover   H: cosechar   1: ojo   2: piernas   3: boca");
    std::wstring s=L"Biomasa: "+std::to_wstring(g_biomass)+L"   Cristales: "+std::to_wstring(g_crystals)+L"   Vision: "+std::to_wstring(g_evo.getVision())+L"   Velocidad: "+std::to_wstring(g_evo.getSpeed());text(dc,28,72,s);
    text(dc,28,95,g_evo.canWalk()?L"Estado: locomocion evolucionada":L"Estado inicial: masa Visceroid rastrera");
}
static void renderFrame(HWND h){RECT rc;GetClientRect(h,&rc);int w=rc.right-rc.left,ht=rc.bottom-rc.top;if(w<=0||ht<=0)return;ensureBackBuffer(h,w,ht);renderScene(g_backDC,rc);HDC wnd=GetDC(h);BitBlt(wnd,0,0,w,ht,g_backDC,0,0,SRCCOPY);ReleaseDC(h,wnd);}
static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
        case WM_ERASEBKGND:return 1;
        case WM_SIZE:destroyBackBuffer();return 0;
        case WM_DESTROY:destroyBackBuffer();PostQuitMessage(0);return 0;
        case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='1')addUpgrade(SPORE_PART_EYE,"eye_basic",25,10,4,0,0,0);if(w=='2')addUpgrade(SPORE_PART_LEG,"legs_basic",50,20,0,4,0,0);if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth_basic",35,15,0,0,3,0);if(w=='H'){int got=g_world.harvest((int)g_creature.getX(),(int)g_creature.getY(),8);g_crystals+=got;if(got>0)g_biomass+=got/2;}return 0;
        case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;
        case WM_KILLFOCUS:for(int i=0;i<256;++i)g_keys[i]=false;return 0;
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}
    }
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){
    QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();
    WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPVoxelMassDemo";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;
    g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion - Visceroid Voxel Mass Demo",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1360,860,NULL,NULL,inst,NULL);if(!g_hwnd)return 2;ShowWindow(g_hwnd,show);
    MSG msg;bool running=true;double acc=0.0,renderAcc=0.0;const double logicStep=1.0/120.0,renderStep=1.0/60.0;
    while(running){
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}
        LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.05)dt=.05;acc+=dt;renderAcc+=dt;
        while(acc>=logicStep){updateGame((float)logicStep);acc-=logicStep;}
        if(renderAcc>=renderStep){renderFrame(g_hwnd);renderAcc=fmod(renderAcc,renderStep);}Sleep(1);
    }
    destroyBackBuffer();return 0;
}
