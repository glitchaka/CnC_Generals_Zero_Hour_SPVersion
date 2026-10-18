#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <string>
#include "SporePrototype/SporeVoxelWorld.h"
#include "SporePrototype/SporeCreature.h"
#include "Spore/SporeEvolution.h"

using namespace SporePrototype;

struct V3{float x,y,z;};
struct BodyVoxel{V3 p; COLORREF c;};
struct Particle{float x,y,z,vx,vy,vz,life;COLORREF c;};

static SporeVoxelWorld g_world;
static SporeCreature g_creature;
static SporeEvolutionState g_evo;
static std::vector<BodyVoxel> g_body;
static std::vector<Particle> g_particles;
static bool g_keys[256]={};
static bool g_showHud=true;
static float g_px=128.0f,g_py=128.0f,g_vx=0.0f,g_vy=0.0f;
static float g_camX=128.0f,g_camY=128.0f,g_zoom=1.45f;
static float g_harvestClock=0.0f;
static int g_targetX=-1,g_targetY=-1,g_biomass=250,g_crystals=100;
static LARGE_INTEGER g_freq={},g_prev={};
static HDC g_backDC=NULL;static HBITMAP g_backBmp=NULL,g_backOld=NULL;static uint32_t* g_pixels=nullptr;static int g_w=0,g_h=0;static HWND g_hwnd=NULL;

static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|(uint32_t)GetBValue(c);} 
static inline void px(int x,int y,COLORREF c){if((unsigned)x<(unsigned)g_w&&(unsigned)y<(unsigned)g_h&&g_pixels)g_pixels[y*g_w+x]=pack(c);} 
static inline void block2(int x,int y,COLORREF c){px(x,y,c);px(x+1,y,c);px(x,y+1,c);px(x+1,y+1,c);} 
static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);r=(std::max)(0,(std::min)(255,r));g=(std::max)(0,(std::min)(255,g));b=(std::max)(0,(std::min)(255,b));return RGB(r,g,b);} 
static COLORREF mixc(COLORREF a,COLORREF b,float t){t=(std::max)(0.0f,(std::min)(1.0f,t));return RGB((int)(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),(int)(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),(int)(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));}
static unsigned hash2(int x,int y,unsigned s=0x9e3779b9u){unsigned h=(unsigned)x*0x85ebca6bu^(unsigned)y*0xc2b2ae35u^s;h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;}
static float h01(int x,int y,unsigned s){return (hash2(x,y,s)&65535)/65535.0f;}
static float sm(float t){return t*t*(3.0f-2.0f*t);} 
static float noise(float x,float y,unsigned s){int x0=(int)floorf(x),y0=(int)floorf(y);float tx=sm(x-x0),ty=sm(y-y0),a=h01(x0,y0,s),b=h01(x0+1,y0,s),c=h01(x0,y0+1,s),d=h01(x0+1,y0+1,s);return (a+(b-a)*tx)+((c+(d-c)*tx)-(a+(b-a)*tx))*ty;}
static float fbm(float x,float y,unsigned s){float f=0,a=.55f;for(int i=0;i<4;++i){f+=noise(x,y,s+i*991u)*a;x*=2.01f;y*=2.03f;a*=.5f;}return f;}

static void destroyBack(){g_pixels=nullptr;if(g_backDC){if(g_backOld)SelectObject(g_backDC,g_backOld);if(g_backBmp)DeleteObject(g_backBmp);DeleteDC(g_backDC);}g_backDC=NULL;g_backBmp=NULL;g_backOld=NULL;g_w=g_h=0;}
static void ensureBack(HWND h,int w,int ht){if(w<=0||ht<=0)return;if(g_backDC&&w==g_w&&ht==g_h)return;destroyBack();HDC wnd=GetDC(h);g_backDC=CreateCompatibleDC(wnd);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-ht;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;g_backBmp=CreateDIBSection(wnd,&bi,DIB_RGB_COLORS,&bits,NULL,0);g_pixels=(uint32_t*)bits;g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);ReleaseDC(h,wnd);g_w=w;g_h=ht;}

static float TW(){return 30.0f*g_zoom;}static float TH(){return 15.0f*g_zoom;}
static POINT worldToScreen(float wx,float wy,float z,int cx,int cy){float rx=wx-g_camX,ry=wy-g_camY;return {(LONG)lroundf(cx+(rx-ry)*TW()*.5f),(LONG)lroundf(cy+(rx+ry)*TH()*.5f-z*g_zoom)};}
static void screenToWorld(float sx,float sy,int cx,int cy,float&wx,float&wy){float u=(sx-cx)/(TW()*.5f),v=(sy-cy)/(TH()*.5f);wx=g_camX+(u+v)*.5f;wy=g_camY+(v-u)*.5f;}

static float resourceField(float wx,float wy){float best=0;for(const CrystalNode&n:g_world.getCrystalNodes()){if(n.reserve<=0)continue;float dx=wx-(n.x+.5f),dy=wy-(n.y+.5f),d=sqrtf(dx*dx+dy*dy)/(n.radius*1.25f);if(d>=1)continue;float f=(1-d)*1.2f+(fbm(wx*.32f,wy*.32f,n.seed)-.55f)*.34f;best=(std::max)(best,f);}return (std::max)(0.0f,best);}
static COLORREF terrainColor(float wx,float wy){float n=fbm(wx*.055f,wy*.055f,31u),f=fbm(wx*.24f,wy*.24f,77u);COLORREF base=mixc(RGB(39,63,39),RGB(73,98,53),n);base=shade(base,.90f+f*.16f);float rf=resourceField(wx,wy);if(rf>.03f){float t=(std::min)(1.0f,rf*1.35f);COLORREF peach=mixc(RGB(117,75,58),RGB(194,126,92),t);base=mixc(base,peach,(std::min)(.96f,t));float vein=fbm(wx*1.2f,wy*1.2f,919u);if(rf>.42f&&vein>.84f)base=mixc(base,RGB(174,181,157),(vein-.84f)*2.0f);}return base;}

static V3 cross3(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static V3 add3(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static V3 mul3(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
static float dot3(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V3 norm3(V3 a){float l=sqrtf(dot3(a,a));return l>.0001f?mul3(a,1.0f/l):V3{0,0,0};}
static V3 rotateAxis(V3 p,V3 axis,float ang){axis=norm3(axis);float c=cosf(ang),s=sinf(ang);return add3(add3(mul3(p,c),mul3(cross3(axis,p),s)),mul3(axis,dot3(axis,p)*(1-c)));}

static void buildBody(){g_body.clear();const float R=14.0f;for(int z=-14;z<=14;z+=2)for(int y=-14;y<=14;y+=2)for(int x=-14;x<=14;x+=2){float rr=sqrtf((float)(x*x+y*y+z*z));if(rr>R)continue;unsigned h=hash2(x*17+y*7,z*11,0xB10Bu);if(rr>R-2.2f || (h&7u)==0){COLORREF c=(h&3u)==0?RGB(89,36,39):((h&3u)==1?RGB(119,45,49):RGB(145,56,59));g_body.push_back({{(float)x,(float)y,(float)z},c});}}}

static void integrateBodyRoll(float dt){float speed=sqrtf(g_vx*g_vx+g_vy*g_vy);if(speed<.0005f)return;const float worldRadius=1.65f;float ang=(speed/worldRadius)*dt;V3 axis={-g_vy,g_vx,0};for(BodyVoxel&v:g_body)v.p=rotateAxis(v.p,axis,ang);}

static void drawBody(int cx,int cy){POINT c=worldToScreen(g_px,g_py,0,cx,cy);struct RV{int sx,sy;float d;COLORREF c;};std::vector<RV> rv;rv.reserve(g_body.size());float speed=sqrtf(g_vx*g_vx+g_vy*g_vy);float squash=(std::min)(.18f,speed*.02f);for(const BodyVoxel&bv:g_body){float lx=bv.p.x*(1.0f+squash),ly=bv.p.y*(1.0f+squash),lz=bv.p.z*(.56f-squash*.35f);int sx=(int)lroundf((lx-ly)*.82f*g_zoom);int sy=(int)lroundf((lx+ly)*.34f*g_zoom-lz*1.15f*g_zoom);float light=(lz+8.0f)/16.0f;COLORREF col=shade(bv.c,.78f+(std::max)(0.0f,(std::min)(1.0f,light))*.42f);rv.push_back({c.x+sx,c.y+sy,lx+ly+lz,col});}std::sort(rv.begin(),rv.end(),[](const RV&a,const RV&b){return a.d<b.d;});for(const RV&q:rv){int s=g_zoom>1.8f?2:1;for(int yy=0;yy<=s;++yy)for(int xx=0;xx<=s;++xx)px(q.sx+xx,q.sy+yy,q.c);}if(g_evo.hasPart(SPORE_PART_EYE)){int ex=(int)(12*g_zoom),ey=(int)(-8*g_zoom),r=(std::max)(2,(int)(4*g_zoom));for(int y=-r;y<=r;++y)for(int x=-r;x<=r;++x)if(x*x+y*y<=r*r)px(c.x+ex+x,c.y+ey+y,RGB(202,202,179));int pr=(std::max)(1,r/3);for(int y=-pr;y<=pr;++y)for(int x=-pr;x<=pr;++x)px(c.x+ex+x,c.y+ey+y,RGB(17,20,18));}}

static bool isCrystal(VoxelMaterial m){return m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;}
static bool findTarget(){float best=9999;int bx=-1,by=-1;for(int y=(int)g_py-5;y<=(int)g_py+5;++y)for(int x=(int)g_px-5;x<=(int)g_px+5;++x){if(!g_world.isInside(x,y)||!isCrystal(g_world.getCell(x,y).material))continue;float dx=x+.5f-g_px,dy=y+.5f-g_py,d=dx*dx+dy*dy;if(d<best&&d<18){best=d;bx=x;by=y;}}g_targetX=bx;g_targetY=by;return bx>=0;}
static void spawnParticles(){if(g_targetX<0)return;for(int i=0;i<6;++i){float a=(hash2(i,(int)(g_px*31+g_py*17),91u)&1023)/1023.0f*6.28318f;Particle p={g_targetX+.5f,g_targetY+.5f,.2f,cosf(a)*1.2f,sinf(a)*1.2f,.5f,1.0f,RGB(190,175,145)};g_particles.push_back(p);}}
static void updateParticles(float dt){for(Particle&p:g_particles){float dx=g_px-p.x,dy=g_py-p.y,d=sqrtf(dx*dx+dy*dy)+.001f;float pull=16.0f/(.7f+d);p.vx+=dx/d*pull*dt;p.vy+=dy/d*pull*dt;p.vz+=(-p.z)*8.0f*dt;p.vx*=powf(.30f,dt);p.vy*=powf(.30f,dt);p.x+=p.vx*dt;p.y+=p.vy*dt;p.z+=p.vz*dt;p.life-=dt;}g_particles.erase(std::remove_if(g_particles.begin(),g_particles.end(),[](const Particle&p){return p.life<=0;}),g_particles.end());}
static void drawParticles(int cx,int cy){for(const Particle&p:g_particles){POINT q=worldToScreen(p.x,p.y,p.z,cx,cy);block2(q.x,q.y,p.c);}}

static void initGame(){g_world.resize(256,256);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(122,122,10,360,1234u);g_world.addCrystalNode(137,132,12,480,777u);g_world.addCrystalNode(117,139,9,300,991u);g_creature.clear();g_creature.setPosition(128,128,0);g_evo.resetToVisceroid();g_px=g_camX=128;g_py=g_camY=128;buildBody();}
static void addUpgrade(SporePartType t,const char*id,int bio,int cry,int vis,int spd,int dmg,int arm){SporePartUpgrade u;u.type=t;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vis;u.speedDelta=spd;u.damageDelta=dmg;u.armorDelta=arm;g_evo.unlock(u,g_biomass,g_crystals);}
static void updateGame(float dt){float ix=0,iy=0;if(g_keys['A']||g_keys[VK_LEFT])ix-=1;if(g_keys['D']||g_keys[VK_RIGHT])ix+=1;if(g_keys['W']||g_keys[VK_UP])iy-=1;if(g_keys['S']||g_keys[VK_DOWN])iy+=1;if(ix&&iy){ix*=.7071067f;iy*=.7071067f;}float accel=g_evo.canWalk()?18.0f:10.5f;g_vx+=ix*accel*dt;g_vy+=iy*accel*dt;float drag=(ix||iy)?2.2f:4.0f;float damp=expf(-drag*dt);g_vx*=damp;g_vy*=damp;float maxSp=g_evo.canWalk()?8.5f:5.2f;float sp=sqrtf(g_vx*g_vx+g_vy*g_vy);if(sp>maxSp){g_vx*=maxSp/sp;g_vy*=maxSp/sp;}g_px+=g_vx*dt;g_py+=g_vy*dt;g_px=(std::max)(8.0f,(std::min)(247.0f,g_px));g_py=(std::max)(8.0f,(std::min)(247.0f,g_py));g_creature.setPosition(g_px,g_py,0);integrateBodyRoll(dt);float follow=1.0f-expf(-dt*5.5f);g_camX+=(g_px-g_camX)*follow;g_camY+=(g_py-g_camY)*follow;static unsigned frame=0;g_world.update(frame++);if(g_keys['H']){if(g_targetX<0||!isCrystal(g_world.getCell(g_targetX,g_targetY).material))findTarget();g_harvestClock+=dt;if(g_targetX>=0&&g_harvestClock>=.12f){g_harvestClock=0;spawnParticles();int got=g_world.harvest(g_targetX,g_targetY,3);g_crystals+=got;if(got)g_biomass+=got/3;if(!got)findTarget();}}else{g_targetX=g_targetY=-1;g_harvestClock=0;}updateParticles(dt);}

static void text(HDC dc,int x,int y,const std::wstring&s){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(218,220,207));TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void render(HWND h){RECT rc;GetClientRect(h,&rc);ensureBack(h,rc.right,rc.bottom);int cx=g_w/2,cy=g_h/2+18;for(int y=0;y<g_h;y+=2)for(int x=0;x<g_w;x+=2){float wx,wy;screenToWorld((float)x,(float)y,cx,cy,wx,wy);block2(x,y,terrainColor(wx,wy));}drawParticles(cx,cy);drawBody(cx,cy);if(g_showHud){text(g_backDC,18,18,L"Biomasa "+std::to_wstring(g_biomass)+L"   Cristales "+std::to_wstring(g_crystals));text(g_backDC,18,40,L"Rueda: zoom   H: absorber   F1: ocultar interfaz");}HDC wnd=GetDC(h);BitBlt(wnd,0,0,g_w,g_h,g_backDC,0,0,SRCCOPY);ReleaseDC(h,wnd);}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBack();return 0;case WM_DESTROY:destroyBack();PostQuitMessage(0);return 0;case WM_MOUSEWHEEL:{short d=GET_WHEEL_DELTA_WPARAM(w);float factor=d>0?1.12f:.893f;g_zoom=(std::max)(.65f,(std::min)(2.8f,g_zoom*factor));return 0;}case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w==VK_ESCAPE){PostMessageW(h,WM_CLOSE,0,0);return 0;}if(w==VK_F1){g_showHud=!g_showHud;return 0;}if(w=='1')addUpgrade(SPORE_PART_EYE,"eye",25,10,4,0,0,0);if(w=='2')addUpgrade(SPORE_PART_LEG,"legs",50,20,0,4,0,0);if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth",35,15,0,0,3,0);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)g_keys[i]=false;return 0;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}}return DefWindowProcW(h,m,w,l);}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int){QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPPhysicsVoxelDemo";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;int sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,0,0,sw,sh,NULL,NULL,inst,NULL);if(!g_hwnd)return 2;MSG msg;double acc=0,racc=0;const double step=1.0/120.0,renderStep=1.0/60.0;bool run=true;while(run){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.05)dt=.05;acc+=dt;racc+=dt;while(acc>=step){updateGame((float)step);acc-=step;}if(racc>=renderStep){render(g_hwnd);racc=fmod(racc,renderStep);}Sleep(1);}destroyBack();return 0;}
