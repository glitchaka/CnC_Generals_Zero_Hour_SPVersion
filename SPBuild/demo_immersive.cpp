#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>
#include "SporePrototype/SporeVoxelWorld.h"
#include "SporePrototype/SporeCreature.h"
#include "Spore/SporeEvolution.h"

using namespace SporePrototype;

struct Particle { float x,y,vx,vy,life; COLORREF c; };

static SporeVoxelWorld g_world;
static SporeCreature g_creature;
static SporeEvolutionState g_evo;
static bool g_keys[256] = {};
static bool g_showHud = true;
static float g_camX=128.0f,g_camY=128.0f,g_zoom=1.35f,g_anim=0.0f,g_harvestClock=0.0f;
static float g_moveX=0.0f,g_moveY=0.0f;
static int g_biomass=250,g_crystals=100,g_targetX=-1,g_targetY=-1;
static LARGE_INTEGER g_freq={},g_prev={};
static HDC g_backDC=NULL; static HBITMAP g_backBmp=NULL,g_backOld=NULL; static uint32_t* g_pixels=nullptr; static int g_w=0,g_h=0;
static HWND g_hwnd=NULL;
static std::vector<Particle> g_particles;

static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|(uint32_t)GetBValue(c);} 
static inline void px(int x,int y,COLORREF c){if((unsigned)x<(unsigned)g_w&&(unsigned)y<(unsigned)g_h&&g_pixels)g_pixels[y*g_w+x]=pack(c);} 
static inline void block2(int x,int y,COLORREF c){px(x,y,c);px(x+1,y,c);px(x,y+1,c);px(x+1,y+1,c);} 
static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);r=(std::max)(0,(std::min)(255,r));g=(std::max)(0,(std::min)(255,g));b=(std::max)(0,(std::min)(255,b));return RGB(r,g,b);} 
static COLORREF mixc(COLORREF a,COLORREF b,float t){t=(std::max)(0.0f,(std::min)(1.0f,t));return RGB((int)(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),(int)(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),(int)(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));}
static unsigned hash2(int x,int y,unsigned s=0x9e3779b9u){unsigned h=(unsigned)x*0x85ebca6bu^(unsigned)y*0xc2b2ae35u^s;h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;}
static float hash01(int x,int y,unsigned s){return (hash2(x,y,s)&65535)/65535.0f;}
static float smoothf(float t){return t*t*(3.0f-2.0f*t);} 
static float valueNoise(float x,float y,unsigned s){int x0=(int)floorf(x),y0=(int)floorf(y);float tx=smoothf(x-x0),ty=smoothf(y-y0);float a=hash01(x0,y0,s),b=hash01(x0+1,y0,s),c=hash01(x0,y0+1,s),d=hash01(x0+1,y0+1,s);float u=a+(b-a)*tx,v=c+(d-c)*tx;return u+(v-u)*ty;}
static float fbm(float x,float y,unsigned s){float f=0,a=.55f;for(int i=0;i<4;++i){f+=valueNoise(x,y,s+i*977u)*a;x*=2.03f;y*=2.01f;a*=.5f;}return f;}

static void destroyBack(){g_pixels=nullptr;if(g_backDC){if(g_backOld)SelectObject(g_backDC,g_backOld);if(g_backBmp)DeleteObject(g_backBmp);DeleteDC(g_backDC);}g_backDC=NULL;g_backBmp=NULL;g_backOld=NULL;g_w=g_h=0;}
static void ensureBack(HWND h,int w,int ht){if(w<=0||ht<=0)return;if(g_backDC&&w==g_w&&ht==g_h)return;destroyBack();HDC wnd=GetDC(h);g_backDC=CreateCompatibleDC(wnd);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-ht;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;g_backBmp=CreateDIBSection(wnd,&bi,DIB_RGB_COLORS,&bits,NULL,0);g_pixels=(uint32_t*)bits;g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);ReleaseDC(h,wnd);g_w=w;g_h=ht;}

static float TW(){return 28.0f*g_zoom;} static float TH(){return 14.0f*g_zoom;}
static POINT worldToScreen(float wx,float wy,int cx,int cy,float z=0){float rx=wx-g_camX,ry=wy-g_camY;return {(LONG)lroundf(cx+(rx-ry)*TW()*.5f),(LONG)lroundf(cy+(rx+ry)*TH()*.5f-z*g_zoom)};}
static void screenToWorld(float sx,float sy,int cx,int cy,float &wx,float &wy){float u=(sx-cx)/(TW()*.5f),v=(sy-cy)/(TH()*.5f);wx=g_camX+(u+v)*.5f;wy=g_camY+(v-u)*.5f;}
static bool isCrystal(VoxelMaterial m){return m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;}

static float resourceField(float wx,float wy){float best=0;for(const CrystalNode&n:g_world.getCrystalNodes()){if(n.reserve<=0)continue;float dx=wx-(n.x+.5f),dy=wy-(n.y+.5f),r=(float)n.radius;float d=sqrtf(dx*dx+dy*dy)/(r*1.18f);if(d>=1.0f)continue;float n1=fbm(wx*.29f,wy*.29f,n.seed);float f=(1.0f-d)*1.22f+(n1-.58f)*.36f;best=(std::max)(best,f);}return (std::max)(0.0f,best);}
static COLORREF terrainColor(float wx,float wy){float large=fbm(wx*.045f,wy*.045f,123u),fine=fbm(wx*.24f,wy*.24f,456u);COLORREF grass=mixc(RGB(43,68,41),RGB(71,96,52),large);grass=shade(grass,.88f+fine*.20f);float soil=fbm(wx*.11f+20,wy*.11f-8,999u);if(soil>.78f)grass=mixc(grass,RGB(70,58,43),(soil-.78f)*2.9f);float rf=resourceField(wx,wy);if(rf>.035f){float t=(std::min)(1.0f,rf*1.30f);COLORREF flesh=mixc(RGB(113,72,57),RGB(190,122,91),.35f+.65f*t);float mott=fbm(wx*.65f,wy*.65f,808u);flesh=shade(flesh,.86f+mott*.20f);grass=mixc(grass,flesh,(std::min)(.96f,t));if(rf>.40f){float vein=fbm(wx*1.35f,wy*1.35f,31337u);if(vein>.82f)grass=mixc(grass,RGB(161,187,176),(vein-.82f)*2.3f);}}return grass;}

static float metaball(float x,float y,float cx,float cy,float rx,float ry){float dx=(x-cx)/rx,dy=(y-cy)/ry;float d=dx*dx+dy*dy;return d<1.0f?1.0f-d:0.0f;}
static float viscField(float x,float y){float speed=sqrtf(g_moveX*g_moveX+g_moveY*g_moveY);float wave=sinf(g_anim*1.6f)*speed, wave2=sinf(g_anim*1.6f+2.1f)*speed;float f=0;f+=metaball(x,y,-13+wave*3,-1,15,8);f+=metaball(x,y,13-wave2*3,-1,15,8);f+=metaball(x,y,0,11+wave*1.2f,11,16);f+=metaball(x,y,0,1,14,9);f-=metaball(x,y,0,-9,4.5f,6.0f)*1.05f;return f;}
static void drawCreature(int cx,int cy){POINT c=worldToScreen(g_creature.getX(),g_creature.getY(),cx,cy);int sxr=(int)(44*g_zoom),syr=(int)(32*g_zoom);for(int sy=-syr;sy<=syr;++sy)for(int sx=-sxr;sx<=sxr;++sx){float lx=sx/g_zoom,ly=sy/g_zoom;float f=viscField(lx,ly);if(f<=.07f)continue;float edge=(std::min)(1.0f,f*.85f);float top=1.0f-(ly+32.0f)/64.0f;float tex=fbm(lx*.11f+g_creature.getX(),ly*.11f+g_creature.getY(),2026u);COLORREF col=mixc(RGB(63,27,30),RGB(147,65,68),.18f+.82f*edge);col=shade(col,.83f+top*.18f+tex*.16f);px(c.x+sx,c.y+sy,col);} 
if(g_evo.hasPart(SPORE_PART_EYE)){int ex=(int)(15*g_zoom),ey=(int)(-7*g_zoom),rr=(int)(5*g_zoom);for(int y=-rr;y<=rr;++y)for(int x=-rr;x<=rr;++x)if(x*x+y*y<=rr*rr)px(c.x+ex+x,c.y+ey+y,RGB(198,199,174));int pr=(std::max)(1,(int)(2*g_zoom));for(int y=-pr;y<=pr;++y)for(int x=-pr;x<=pr;++x)px(c.x+ex+x,c.y+ey+y,RGB(17,20,18));}}

static bool findTarget(){float gx=g_creature.getX(),gy=g_creature.getY(),best=9999;int bx=-1,by=-1;for(int y=(int)gy-5;y<=(int)gy+5;++y)for(int x=(int)gx-5;x<=(int)gx+5;++x){if(!g_world.isInside(x,y)||!isCrystal(g_world.getCell(x,y).material))continue;float dx=x+.5f-gx,dy=y+.5f-gy,d=dx*dx+dy*dy;if(d<best&&d<18.0f){best=d;bx=x;by=y;}}g_targetX=bx;g_targetY=by;return bx>=0;}
static void spawnHarvestParticles(){if(g_targetX<0)return;for(int i=0;i<7;++i){float a=(hash2(i,(int)g_anim,77u)&1023)/1023.0f*6.28318f;float s=.7f+((hash2(i,99,5u)>>8)&255)/255.0f*1.5f;Particle p;p.x=g_targetX+.5f;p.y=g_targetY+.5f;p.vx=cosf(a)*s;p.vy=sinf(a)*s;p.life=.65f+.15f*i/7.0f;p.c=RGB(171,198,187);g_particles.push_back(p);}}
static void updateParticles(float dt){float tx=g_creature.getX(),ty=g_creature.getY();for(Particle&p:g_particles){float dx=tx-p.x,dy=ty-p.y;float d=sqrtf(dx*dx+dy*dy)+.001f;float pull=18.0f/(.8f+d);p.vx+=dx/d*pull*dt;p.vy+=dy/d*pull*dt;p.vx*=powf(.28f,dt);p.vy*=powf(.28f,dt);p.x+=p.vx*dt;p.y+=p.vy*dt;p.life-=dt;}g_particles.erase(std::remove_if(g_particles.begin(),g_particles.end(),[](const Particle&p){return p.life<=0;}),g_particles.end());}
static void drawParticles(int cx,int cy){for(const Particle&p:g_particles){POINT q=worldToScreen(p.x,p.y,cx,cy,3.0f*sinf((1-p.life)*6.0f));int r=g_zoom>1.45f?2:1;for(int y=-r;y<=r;++y)for(int x=-r;x<=r;++x)px(q.x+x,q.y+y,p.c);}}

static void initGame(){g_world.resize(256,256);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(122,122,10,360,1234u);g_world.addCrystalNode(137,132,12,480,777u);g_world.addCrystalNode(117,139,9,300,991u);g_creature.clear();g_creature.setPosition(128,128,0);g_camX=128;g_camY=128;g_evo.resetToVisceroid();}
static void addUpgrade(SporePartType t,const char*id,int bio,int cry,int vis,int spd,int dmg,int arm){SporePartUpgrade u;u.type=t;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vis;u.speedDelta=spd;u.damageDelta=dmg;u.armorDelta=arm;g_evo.unlock(u,g_biomass,g_crystals);} 
static void updateGame(float dt){float dx=0,dy=0;if(g_keys['A']||g_keys[VK_LEFT])dx-=1;if(g_keys['D']||g_keys[VK_RIGHT])dx+=1;if(g_keys['W']||g_keys[VK_UP])dy-=1;if(g_keys['S']||g_keys[VK_DOWN])dy+=1;if(dx&&dy){dx*=.7071067f;dy*=.7071067f;}g_moveX=dx;g_moveY=dy;float sp=4.7f+(float)g_evo.getSpeed()*.45f;if(!g_evo.canWalk())sp*=.68f;if(dx||dy)g_creature.move(dx,dy,dt*sp);float x=(std::max)(8.0f,(std::min)(247.0f,g_creature.getX())),y=(std::max)(8.0f,(std::min)(247.0f,g_creature.getY()));g_creature.setPosition(x,y,0);float f=1.0f-expf(-dt*6.5f);g_camX+=(x-g_camX)*f;g_camY+=(y-g_camY)*f;g_anim+=dt*((dx||dy)?5.5f:1.3f);static unsigned frame=0;g_world.update(frame++);if(g_keys['H']){if(g_targetX<0||!isCrystal(g_world.getCell(g_targetX,g_targetY).material))findTarget();g_harvestClock+=dt;if(g_targetX>=0&&g_harvestClock>=.12f){g_harvestClock=0;spawnHarvestParticles();int got=g_world.harvest(g_targetX,g_targetY,3);g_crystals+=got;if(got)g_biomass+=got/3;if(!got)findTarget();}}else{g_targetX=g_targetY=-1;g_harvestClock=0;}updateParticles(dt);}

static void text(HDC dc,int x,int y,const std::wstring&s){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(218,220,207));TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void render(HWND h){RECT rc;GetClientRect(h,&rc);ensureBack(h,rc.right,rc.bottom);int cx=g_w/2,cy=g_h/2+25;for(int y=0;y<g_h;y+=2)for(int x=0;x<g_w;x+=2){float wx,wy;screenToWorld((float)x,(float)y,cx,cy,wx,wy);COLORREF c=terrainColor(wx,wy);block2(x,y,c);}drawParticles(cx,cy);drawCreature(cx,cy);if(g_showHud){text(g_backDC,20,20,L"Biomasa "+std::to_wstring(g_biomass)+L"   Mineral "+std::to_wstring(g_crystals));text(g_backDC,20,42,L"rueda: zoom   H: absorber   F1: HUD   Esc: salir");}HDC wnd=GetDC(h);BitBlt(wnd,0,0,g_w,g_h,g_backDC,0,0,SRCCOPY);ReleaseDC(h,wnd);}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBack();return 0;case WM_DESTROY:destroyBack();PostQuitMessage(0);return 0;case WM_MOUSEWHEEL:{int d=GET_WHEEL_DELTA_WPARAM(w);float old=g_zoom;g_zoom*=d>0?1.12f:.892857f;g_zoom=(std::max)(.60f,(std::min)(2.80f,g_zoom));if(fabsf(old-g_zoom)<.0001f)g_zoom=old;return 0;}case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w==VK_ESCAPE)DestroyWindow(h);if(w==VK_F1){g_showHud=!g_showHud;g_keys[w]=false;}if(w=='1')addUpgrade(SPORE_PART_EYE,"eye",25,10,4,0,0,0);if(w=='2')addUpgrade(SPORE_PART_LEG,"legs",50,20,0,4,0,0);if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth",35,15,0,0,3,0);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)g_keys[i]=false;return 0;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}}return DefWindowProcW(h,m,w,l);} 

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int){QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPImmersiveVoxel";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=NULL;if(!RegisterClassW(&wc))return 1;RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int ww=(std::min)(1600,(int)(work.right-work.left)),hh=(std::min)(960,(int)(work.bottom-work.top));int x=work.left+(work.right-work.left-ww)/2,y=work.top+(work.bottom-work.top-hh)/2;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP|WS_VISIBLE,x,y,ww,hh,NULL,NULL,inst,NULL);if(!g_hwnd)return 2;MSG msg;bool run=true;double acc=0,ra=0;const double step=1.0/120.0,renderStep=1.0/60.0;while(run){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.05)dt=.05;acc+=dt;ra+=dt;while(acc>=step){updateGame((float)step);acc-=step;}if(ra>=renderStep){render(g_hwnd);ra=fmod(ra,renderStep);}Sleep(1);}destroyBack();return 0;}
