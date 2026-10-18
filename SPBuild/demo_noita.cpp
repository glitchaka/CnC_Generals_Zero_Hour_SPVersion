#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string>
#include <algorithm>
#include "SporePrototype/SporeVoxelWorld.h"
#include "SporePrototype/SporeCreature.h"
#include "Spore/SporeEvolution.h"

using namespace SporePrototype;

static SporeVoxelWorld g_world;
static SporeCreature g_creature;
static SporeEvolutionState g_evo;
static bool g_keys[256]={};
static int g_biomass=250,g_crystals=100;
static float g_camX=128.0f,g_camY=128.0f;
static float g_anim=0.0f,g_harvestClock=0.0f;
static int g_targetX=-1,g_targetY=-1;
static LARGE_INTEGER g_freq={},g_prev={};
static HDC g_backDC=NULL; static HBITMAP g_backBmp=NULL,g_backOld=NULL;
static uint32_t* g_pixels=nullptr; static int g_w=0,g_h=0;
static HWND g_hwnd=NULL;

static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|GetBValue(c);}
static inline void px(int x,int y,COLORREF c){if((unsigned)x<(unsigned)g_w&&(unsigned)y<(unsigned)g_h&&g_pixels)g_pixels[y*g_w+x]=pack(c);}
static inline void dot2(int x,int y,COLORREF c){px(x,y,c);px(x+1,y,c);px(x,y+1,c);px(x+1,y+1,c);}
static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);r=(std::max)(0,(std::min)(255,r));g=(std::max)(0,(std::min)(255,g));b=(std::max)(0,(std::min)(255,b));return RGB(r,g,b);}
static unsigned hash2(int x,int y,unsigned s=0x9e3779b9u){unsigned h=(unsigned)x*0x85ebca6bu^(unsigned)y*0xc2b2ae35u^s;h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;}

static void destroyBack(){g_pixels=nullptr;if(g_backDC){if(g_backOld)SelectObject(g_backDC,g_backOld);if(g_backBmp)DeleteObject(g_backBmp);DeleteDC(g_backDC);}g_backDC=NULL;g_backBmp=NULL;g_backOld=NULL;g_w=g_h=0;}
static void ensureBack(HWND h,int w,int ht){if(w<=0||ht<=0)return;if(g_backDC&&w==g_w&&ht==g_h)return;destroyBack();HDC wnd=GetDC(h);g_backDC=CreateCompatibleDC(wnd);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-ht;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;g_backBmp=CreateDIBSection(wnd,&bi,DIB_RGB_COLORS,&bits,NULL,0);g_pixels=(uint32_t*)bits;g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);ReleaseDC(h,wnd);g_w=w;g_h=ht;}

static const float TW=28.0f,TH=14.0f;
static POINT worldToScreen(float wx,float wy,int cx,int cy){float rx=wx-g_camX,ry=wy-g_camY;POINT p={(LONG)lroundf(cx+(rx-ry)*(TW*.5f)),(LONG)lroundf(cy+(rx+ry)*(TH*.5f))};return p;}
static void fillDiamond(int cx,int cy,int hw,int hh,COLORREF c){for(int y=-hh;y<=hh;++y){int span=(int)((1.0f-fabsf((float)y/(float)(hh+1)))*hw);for(int x=-span;x<=span;++x)px(cx+x,cy+y,c);}}
static COLORREF matBase(VoxelMaterial m){switch(m){case VOXEL_SOIL:return RGB(87,71,51);case VOXEL_ROCK:return RGB(92,92,86);case VOXEL_PEACH_BIOMASS:return RGB(202,132,100);case VOXEL_WATER:return RGB(52,96,139);default:return RGB(74,113,61);}}
static COLORREF crystalColor(VoxelMaterial m){switch(m){case VOXEL_CRYSTAL_CYAN:return RGB(63,225,230);case VOXEL_CRYSTAL_MAGENTA:return RGB(221,70,180);case VOXEL_CRYSTAL_GREEN:return RGB(104,224,111);default:return RGB(0,0,0);}}
static bool isCrystal(VoxelMaterial m){return m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;}

static void drawCell(int wx,int wy,int cx,int cy){POINT p=worldToScreen((float)wx+.5f,(float)wy+.5f,cx,cy);VoxelMaterial m=g_world.isInside(wx,wy)?g_world.getCell(wx,wy).material:VOXEL_GRASS;COLORREF base=isCrystal(m)?RGB(209,139,104):matBase(m);unsigned h=hash2(wx,wy);fillDiamond(p.x,p.y,(int)(TW*.5f)+1,(int)(TH*.5f)+1,shade(base,.94f+((h&7)*.012f)));
    if(m==VOXEL_PEACH_BIOMASS||isCrystal(m)){COLORREF dust=isCrystal(m)?crystalColor(m):RGB(231,161,121);for(int i=0;i<22;++i){unsigned q=hash2(wx*31+i,wy*17-i,h);int ox=(int)(q%21)-10;int oy=(int)((q>>8)%9)-4;if(abs(ox)*4+abs(oy)*9<44){if(isCrystal(m)&&i<13)dot2(p.x+ox,p.y+oy,dust);else px(p.x+ox,p.y+oy,shade(base,1.12f));}}}
}

static bool bodyPoint(float x,float y){float moving=(fabsf(g_creature.getX()-g_camX)+fabsf(g_creature.getY()-g_camY))>.01f?1.0f:0.35f;float a=g_anim*moving;float l=sinf(a)*1.8f;float r=cosf(a*.91f)*1.5f;auto e=[](float x,float y,float cx,float cy,float rx,float ry){float dx=(x-cx)/rx,dy=(y-cy)/ry;return dx*dx+dy*dy<=1.0f;};return e(x,y,-8+l,-1,10,7)||e(x,y,8-r,-1,10,7)||e(x,y,0,7,8,12)||e(x,y,0,1,11,7);}
static void drawCreature(int cx,int cy){POINT c=worldToScreen(g_creature.getX(),g_creature.getY(),cx,cy);for(int y=-18;y<=18;++y)for(int x=-22;x<=22;++x){if(!bodyPoint((float)x*.65f,(float)y*.65f))continue;unsigned h=hash2(x,y,1234u);float edge=(float)y/18.0f;COLORREF col=(h&3)?RGB(143,59,64):RGB(116,45,50);col=shade(col,1.05f-edge*.13f);px(c.x+x,c.y+y,col);if((h&15)==0)px(c.x+x+1,c.y,shade(col,1.18f));}
    if(g_evo.hasPart(SPORE_PART_EYE)){for(int y=-5;y<2;++y)for(int x=6;x<13;++x){float dx=(x-9)/3.5f,dy=(y+2)/3.5f;if(dx*dx+dy*dy<=1){px(c.x+x,c.y+y,RGB(213,216,195));}}dot2(c.x+10,c.y-2,RGB(18,23,22));}
}

static bool findHarvestTarget(){float pxw=g_creature.getX(),pyw=g_creature.getY();float best=9999.0f;int bx=-1,by=-1;for(int y=(int)pyw-4;y<=(int)pyw+4;++y)for(int x=(int)pxw-4;x<=(int)pxw+4;++x){if(!g_world.isInside(x,y)||!isCrystal(g_world.getCell(x,y).material))continue;float dx=x+.5f-pxw,dy=y+.5f-pyw,d=dx*dx+dy*dy;if(d<best&&d<=12.25f){best=d;bx=x;by=y;}}g_targetX=bx;g_targetY=by;return bx>=0;}
static void drawHarvestFX(int cx,int cy){if(g_targetX<0)return;POINT a=worldToScreen(g_creature.getX(),g_creature.getY(),cx,cy),b=worldToScreen(g_targetX+.5f,g_targetY+.5f,cx,cy);VoxelMaterial m=g_world.getCell(g_targetX,g_targetY).material;COLORREF cc=crystalColor(m);for(int i=0;i<14;++i){float t=fmodf(g_anim*.13f+i/14.0f,1.0f);float bend=sinf(t*3.14159f)*6.0f;int x=(int)lroundf(b.x+(a.x-b.x)*t);int y=(int)lroundf(b.y+(a.y-b.y)*t-bend);dot2(x,y,cc);} }

static void initGame(){g_world.resize(256,256);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(123,123,8,320,1234u);g_world.addCrystalNode(136,132,10,420,777u);g_world.addCrystalNode(117,138,7,240,991u);g_creature.clear();g_creature.setPosition(128.0f,128.0f,0);g_camX=128.0f;g_camY=128.0f;g_evo.resetToVisceroid();}
static void updateGame(float dt){float dx=0,dy=0;if(g_keys['A']||g_keys[VK_LEFT])dx-=1;if(g_keys['D']||g_keys[VK_RIGHT])dx+=1;if(g_keys['W']||g_keys[VK_UP])dy-=1;if(g_keys['S']||g_keys[VK_DOWN])dy+=1;if(dx&&dy){dx*=.7071067f;dy*=.7071067f;}float speed=5.2f+(float)g_evo.getSpeed()*.45f;if(!g_evo.canWalk())speed*=.68f;if(dx||dy)g_creature.move(dx,dy,dt*speed);float x=(std::max)(8.0f,(std::min)(247.0f,g_creature.getX())),y=(std::max)(8.0f,(std::min)(247.0f,g_creature.getY()));g_creature.setPosition(x,y,0);float f=1.0f-expf(-dt*8.0f);g_camX+=(x-g_camX)*f;g_camY+=(y-g_camY)*f;g_anim+=dt*((dx||dy)?10.0f:2.0f);static unsigned frame=0;g_world.update(frame++);
    if(g_keys['H']){if(g_targetX<0||!isCrystal(g_world.getCell(g_targetX,g_targetY).material))findHarvestTarget();g_harvestClock+=dt;if(g_targetX>=0&&g_harvestClock>=.09f){g_harvestClock=0;int got=g_world.harvest(g_targetX,g_targetY,4);g_crystals+=got;if(got)g_biomass+=got/3;if(got==0)findHarvestTarget();}}else{g_targetX=g_targetY=-1;g_harvestClock=0;}}

static void addUpgrade(SporePartType t,const char*id,int bio,int cry,int vis,int spd,int dmg,int arm){SporePartUpgrade u;u.type=t;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vis;u.speedDelta=spd;u.damageDelta=dmg;u.armorDelta=arm;g_evo.unlock(u,g_biomass,g_crystals);}
static void text(HDC dc,int x,int y,const std::wstring&s){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(238,238,232));TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void render(HWND h){RECT rc;GetClientRect(h,&rc);ensureBack(h,rc.right,rc.bottom);std::fill(g_pixels,g_pixels+(size_t)g_w*g_h,pack(RGB(74,113,61)));int cx=g_w/2,cy=g_h/2+20;int R=(int)(g_w/TW+g_h/TH)+6;int ix=(int)floorf(g_camX),iy=(int)floorf(g_camY);for(int s=-2*R;s<=2*R;++s)for(int rx=-R;rx<=R;++rx){int ry=s-rx;if(ry<-R||ry>R)continue;drawCell(ix+rx,iy+ry,cx,cy);}drawCreature(cx,cy);if(g_keys['H'])drawHarvestFX(cx,cy);RECT hud={18,18,670,98};FillRect(g_backDC,&hud,(HBRUSH)GetStockObject(BLACK_BRUSH));text(g_backDC,30,27,L"SPVersion - granular voxel prototype");text(g_backDC,30,49,L"WASD/flechas: mover   mantener H: absorber recurso   1: ojo   2: piernas   3: boca");std::wstring s=L"Biomasa: "+std::to_wstring(g_biomass)+L"   Cristales: "+std::to_wstring(g_crystals);text(g_backDC,30,71,s);HDC wnd=GetDC(h);BitBlt(wnd,0,0,g_w,g_h,g_backDC,0,0,SRCCOPY);ReleaseDC(h,wnd);}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBack();return 0;case WM_DESTROY:destroyBack();PostQuitMessage(0);return 0;case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='1')addUpgrade(SPORE_PART_EYE,"eye_basic",25,10,4,0,0,0);if(w=='2')addUpgrade(SPORE_PART_LEG,"legs_basic",50,20,0,4,0,0);if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth_basic",35,15,0,0,3,0);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)g_keys[i]=false;return 0;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}}return DefWindowProcW(h,m,w,l);}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPNoitaVoxelDemo";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion - Granular Voxel Creature Demo",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1440,900,NULL,NULL,inst,NULL);if(!g_hwnd)return 2;ShowWindow(g_hwnd,show);MSG msg;double acc=0,racc=0;const double step=1.0/120.0,renderStep=1.0/60.0;bool run=true;while(run){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.05)dt=.05;acc+=dt;racc+=dt;while(acc>=step){updateGame((float)step);acc-=step;}if(racc>=renderStep){render(g_hwnd);racc=fmod(racc,renderStep);}Sleep(1);}destroyBack();return 0;}
