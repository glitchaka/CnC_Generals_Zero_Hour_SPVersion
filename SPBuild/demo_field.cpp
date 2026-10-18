#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string>
#include <algorithm>
#include <vector>
#include "SporePrototype/SporeVoxelWorld.h"
#include "SporePrototype/SporeCreature.h"
#include "Spore/SporeEvolution.h"

using namespace SporePrototype;

static SporeVoxelWorld g_world;
static SporeCreature g_creature;
static SporeEvolutionState g_evo;
static bool g_keys[256]={};
static int g_biomass=250,g_crystals=100;
static float g_camX=128.0f,g_camY=128.0f,g_anim=0.0f,g_harvestClock=0.0f;
static int g_targetX=-1,g_targetY=-1;
static LARGE_INTEGER g_freq={},g_prev={};
static HDC g_backDC=NULL; static HBITMAP g_backBmp=NULL,g_backOld=NULL;
static uint32_t* g_pixels=nullptr; static int g_w=0,g_h=0;

static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|(uint32_t)GetBValue(c);}
static inline void px(int x,int y,COLORREF c){if((unsigned)x<(unsigned)g_w&&(unsigned)y<(unsigned)g_h&&g_pixels)g_pixels[y*g_w+x]=pack(c);}
static inline void block2(int x,int y,COLORREF c){px(x,y,c);px(x+1,y,c);px(x,y+1,c);px(x+1,y+1,c);}
static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);r=(std::max)(0,(std::min)(255,r));g=(std::max)(0,(std::min)(255,g));b=(std::max)(0,(std::min)(255,b));return RGB(r,g,b);}
static COLORREF mixc(COLORREF a,COLORREF b,float t){t=(std::max)(0.0f,(std::min)(1.0f,t));return RGB((int)(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),(int)(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),(int)(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));}
static unsigned hash2(int x,int y,unsigned s=0x9e3779b9u){unsigned h=(unsigned)x*0x85ebca6bu^(unsigned)y*0xc2b2ae35u^s;h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;}

static void destroyBack(){g_pixels=nullptr;if(g_backDC){if(g_backOld)SelectObject(g_backDC,g_backOld);if(g_backBmp)DeleteObject(g_backBmp);DeleteDC(g_backDC);}g_backDC=NULL;g_backBmp=NULL;g_backOld=NULL;g_w=g_h=0;}
static void ensureBack(HWND h,int w,int ht){if(w<=0||ht<=0)return;if(g_backDC&&w==g_w&&ht==g_h)return;destroyBack();HDC wnd=GetDC(h);g_backDC=CreateCompatibleDC(wnd);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-ht;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;g_backBmp=CreateDIBSection(wnd,&bi,DIB_RGB_COLORS,&bits,NULL,0);g_pixels=(uint32_t*)bits;g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);ReleaseDC(h,wnd);g_w=w;g_h=ht;}

static const float TW=22.0f,TH=11.0f;
static void screenToWorld(float sx,float sy,int cx,int cy,float &wx,float &wy){float u=(sx-cx)/(TW*.5f),v=(sy-cy)/(TH*.5f);wx=g_camX+(u+v)*.5f;wy=g_camY+(v-u)*.5f;}
static POINT worldToScreen(float wx,float wy,int cx,int cy){float rx=wx-g_camX,ry=wy-g_camY;return {(LONG)lroundf(cx+(rx-ry)*(TW*.5f)),(LONG)lroundf(cy+(rx+ry)*(TH*.5f))};}

static COLORREF crystalColor(VoxelMaterial m){if(m==VOXEL_CRYSTAL_MAGENTA)return RGB(211,77,172);if(m==VOXEL_CRYSTAL_GREEN)return RGB(94,209,101);return RGB(66,205,220);}
static bool isCrystal(VoxelMaterial m){return m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;}

static float resourceField(float wx,float wy,VoxelMaterial &mat){float best=0.0f;mat=VOXEL_EMPTY;for(const CrystalNode &n:g_world.getCrystalNodes()){if(n.reserve<=0)continue;float dx=wx-(n.x+.5f),dy=wy-(n.y+.5f);float r=(float)n.radius;float d=sqrtf(dx*dx+dy*dy);float t=1.0f-d/(r*1.22f);if(t<=0)continue;unsigned h=hash2((int)floorf(wx*4.0f),(int)floorf(wy*4.0f),n.seed);float noise=((h&255)/255.0f-.5f)*.28f;float f=t+noise;if(f>best){best=f;unsigned k=(h>>8)%3;mat=k==0?VOXEL_CRYSTAL_CYAN:(k==1?VOXEL_CRYSTAL_MAGENTA:VOXEL_CRYSTAL_GREEN);}}return (std::max)(0.0f,best);}

static COLORREF terrainColor(float wx,float wy){int ix=(int)floorf(wx),iy=(int)floorf(wy);unsigned h=hash2(ix,iy,0x1234u);COLORREF grass=mixc(RGB(69,105,58),RGB(82,122,67),((h>>4)&31)/31.0f);if(((h>>9)&63)<6)grass=mixc(grass,RGB(92,72,51),.58f);VoxelMaterial cm=VOXEL_EMPTY;float rf=resourceField(wx,wy,cm);if(rf>.08f){float edge=(std::min)(1.0f,rf*1.45f);COLORREF peach=mixc(RGB(185,117,88),RGB(224,151,113),edge);grass=mixc(grass,peach,(std::min)(.92f,edge));if(rf>.42f&&((hash2((int)(wx*7),(int)(wy*7),0x777u)&31)<3)){grass=mixc(grass,crystalColor(cm),.62f);}}return grass;}

static float blob(float x,float y,float cx,float cy,float rx,float ry){float dx=(x-cx)/rx,dy=(y-cy)/ry;float d=dx*dx+dy*dy;return d<1.0f?1.0f-d:0.0f;}
static float viscField(float x,float y){float moving=(fabsf(g_keys['A']?1.0f:0.0f)+fabsf(g_keys['D']?1.0f:0.0f)+fabsf(g_keys['W']?1.0f:0.0f)+fabsf(g_keys['S']?1.0f:0.0f))>0?1.0f:0.25f;float a=g_anim*moving;float s1=sinf(a)*1.7f,s2=sinf(a+2.2f)*1.4f,s3=sinf(a*.72f+1.1f)*1.1f;float f=0;f+=(std::max)(0.0f,blob(x,y,-11+s1,-2,14,8));f+=(std::max)(0.0f,blob(x,y,11-s2,-2,14,8));f+=(std::max)(0.0f,blob(x,y,0,9+s3,10,14));f+=(std::max)(0.0f,blob(x,y,0,1,13,9));f-=blob(x,y,0,-8,4.0f,6.0f)*.95f;return f;}

static void drawCreature(int cx,int cy){POINT c=worldToScreen(g_creature.getX(),g_creature.getY(),cx,cy);for(int sy=-25;sy<=24;++sy)for(int sx=-34;sx<=34;++sx){float f=viscField((float)sx,(float)sy);if(f<=.08f)continue;unsigned h=hash2(sx,sy,0xabcdu);float tex=((h&255)/255.0f-.5f)*.18f;float top=(1.0f-(sy+25)/50.0f)*.12f;COLORREF col=mixc(RGB(105,42,47),RGB(171,68,72),(std::min)(1.0f,f*.72f+.25f));col=shade(col,1.0f+tex+top);px(c.x+sx,c.y+sy,col);if(f>.6f&&(h&31)==0)px(c.x+sx,c.y+sy-1,shade(col,1.18f));}
if(g_evo.hasPart(SPORE_PART_EYE)){for(int y=-10;y<-2;++y)for(int x=9;x<18;++x){float dx=(x-13.5f)/4.3f,dy=(y+6.0f)/4.0f;if(dx*dx+dy*dy<1)px(c.x+x,c.y+y,RGB(215,213,190));}for(int y=-8;y<-4;++y)for(int x=13;x<16;++x)px(c.x+x,c.y+y,RGB(20,22,20));}
}

static bool findTarget(){float gx=g_creature.getX(),gy=g_creature.getY(),best=9999.0f;int bx=-1,by=-1;for(int y=(int)gy-5;y<=(int)gy+5;++y)for(int x=(int)gx-5;x<=(int)gx+5;++x){if(!g_world.isInside(x,y)||!isCrystal(g_world.getCell(x,y).material))continue;float dx=x+.5f-gx,dy=y+.5f-gy,d=dx*dx+dy*dy;if(d<best&&d<16.0f){best=d;bx=x;by=y;}}g_targetX=bx;g_targetY=by;return bx>=0;}
static void harvestFX(int cx,int cy){if(g_targetX<0)return;POINT a=worldToScreen(g_creature.getX(),g_creature.getY(),cx,cy),b=worldToScreen(g_targetX+.5f,g_targetY+.5f,cx,cy);VoxelMaterial m=g_world.getCell(g_targetX,g_targetY).material;COLORREF cc=crystalColor(m);for(int i=0;i<18;++i){float t=fmodf(g_anim*.17f+i/18.0f,1.0f);float arc=sinf(t*3.14159f)*10.0f;int x=(int)lroundf(b.x+(a.x-b.x)*t);int y=(int)lroundf(b.y+(a.y-b.y)*t-arc);block2(x,y,cc);}float pulse=.5f+.5f*sinf(g_anim*2.0f);for(int r=0;r<3;++r){int rr=(int)(5+r*4+pulse*2);for(int q=0;q<24;++q){float ang=q*6.28318f/24.0f;px(a.x+(int)(cosf(ang)*rr),a.y+(int)(sinf(ang)*rr*.45f),shade(cc,.75f));}}}

static void initGame(){g_world.resize(256,256);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(122,122,10,360,1234u);g_world.addCrystalNode(137,132,12,480,777u);g_world.addCrystalNode(117,139,9,300,991u);g_creature.clear();g_creature.setPosition(128,128,0);g_camX=128;g_camY=128;g_evo.resetToVisceroid();}
static void updateGame(float dt){float dx=0,dy=0;if(g_keys['A']||g_keys[VK_LEFT])dx-=1;if(g_keys['D']||g_keys[VK_RIGHT])dx+=1;if(g_keys['W']||g_keys[VK_UP])dy-=1;if(g_keys['S']||g_keys[VK_DOWN])dy+=1;if(dx&&dy){dx*=.7071067f;dy*=.7071067f;}float speed=5.0f+(float)g_evo.getSpeed()*.45f;if(!g_evo.canWalk())speed*=.70f;if(dx||dy)g_creature.move(dx,dy,dt*speed);float x=(std::max)(8.0f,(std::min)(247.0f,g_creature.getX())),y=(std::max)(8.0f,(std::min)(247.0f,g_creature.getY()));g_creature.setPosition(x,y,0);float f=1.0f-expf(-dt*8.0f);g_camX+=(x-g_camX)*f;g_camY+=(y-g_camY)*f;g_anim+=dt*((dx||dy)?8.5f:2.0f);static unsigned frame=0;g_world.update(frame++);if(g_keys['H']){if(g_targetX<0||!isCrystal(g_world.getCell(g_targetX,g_targetY).material))findTarget();g_harvestClock+=dt;if(g_targetX>=0&&g_harvestClock>=.08f){g_harvestClock=0;int got=g_world.harvest(g_targetX,g_targetY,3);g_crystals+=got;if(got)g_biomass+=got/3;if(!got)findTarget();}}else{g_targetX=g_targetY=-1;g_harvestClock=0;}}

static void addUpgrade(SporePartType t,const char*id,int bio,int cry,int vis,int spd,int dmg,int arm){SporePartUpgrade u;u.type=t;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vis;u.speedDelta=spd;u.damageDelta=dmg;u.armorDelta=arm;g_evo.unlock(u,g_biomass,g_crystals);}
static void text(HDC dc,int x,int y,const std::wstring&s){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(238,238,232));TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void render(HWND h){RECT rc;GetClientRect(h,&rc);ensureBack(h,rc.right,rc.bottom);int cx=g_w/2,cy=g_h/2+20;for(int y=0;y<g_h;y+=2)for(int x=0;x<g_w;x+=2){float wx,wy;screenToWorld((float)x,(float)y,cx,cy,wx,wy);COLORREF c=terrainColor(wx,wy);unsigned hsh=hash2((int)(wx*5),(int)(wy*5),0x88u);c=shade(c,.96f+((hsh&15)/15.0f)*.07f);block2(x,y,c);}drawCreature(cx,cy);if(g_keys['H'])harvestFX(cx,cy);RECT hud={18,18,660,98};HBRUSH b=CreateSolidBrush(RGB(20,23,20));FillRect(g_backDC,&hud,b);DeleteObject(b);text(g_backDC,30,27,L"SPVersion - continuous voxel field");text(g_backDC,30,49,L"WASD/flechas: mover   mantener H: absorber   1: ojo   2: piernas   3: boca");text(g_backDC,30,71,L"Biomasa: "+std::to_wstring(g_biomass)+L"   Cristales: "+std::to_wstring(g_crystals));HDC wnd=GetDC(h);BitBlt(wnd,0,0,g_w,g_h,g_backDC,0,0,SRCCOPY);ReleaseDC(h,wnd);}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBack();return 0;case WM_DESTROY:destroyBack();PostQuitMessage(0);return 0;case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='1')addUpgrade(SPORE_PART_EYE,"eye",25,10,4,0,0,0);if(w=='2')addUpgrade(SPORE_PART_LEG,"legs",50,20,0,4,0,0);if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth",35,15,0,0,3,0);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)g_keys[i]=false;return 0;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPFieldDemo";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;HWND h=CreateWindowExW(0,wc.lpszClassName,L"SPVersion - Voxel Field Demo",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1440,900,NULL,NULL,inst,NULL);if(!h)return 2;ShowWindow(h,show);MSG msg;double acc=0,racc=0;const double step=1.0/120.0,renderStep=1.0/60.0;bool run=true;while(run){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.05)dt=.05;acc+=dt;racc+=dt;while(acc>=step){updateGame((float)step);acc-=step;}if(racc>=renderStep){render(h);racc=fmod(racc,renderStep);}Sleep(1);}destroyBack();return 0;}
