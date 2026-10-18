#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <string>
#include "SporePrototype/SporeVoxelWorld.h"
#include "Spore/SporeEvolution.h"

using namespace SporePrototype;

struct V3 { float x,y,z; };
struct Particle { V3 p,v,f; float mass,radius,restRadius; COLORREF c; bool contact; };
struct Spring { int a,b; float rest,k,damp; };
struct HarvestParticle { V3 p,v; float life; };
struct ScreenParticle { int x,y,r; float depth,light; COLORREF c; };

static SporeVoxelWorld g_world;
static SporeEvolutionState g_evo;
static std::vector<Particle> g_body;
static std::vector<Spring> g_springs;
static std::vector<HarvestParticle> g_harvestFx;
static bool g_keys[256]={};
static bool g_showHud=true;
static int g_biomass=250,g_crystals=100,g_targetX=-1,g_targetY=-1;
static float g_harvestClock=0.0f;
static float g_camX=128.0f,g_camY=128.0f,g_zoom=1.55f;
static LARGE_INTEGER g_freq={},g_prev={};
static HDC g_backDC=NULL; static HBITMAP g_backBmp=NULL,g_backOld=NULL;
static uint32_t* g_pixels=nullptr; static int g_w=0,g_h=0; static HWND g_hwnd=NULL;

static inline V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static inline V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static inline V3 mul(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
static inline float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline float len(V3 a){return sqrtf(dot(a,a));}
static inline V3 norm(V3 a){float l=len(a);return l>1e-5f?mul(a,1.0f/l):V3{0,0,0};}
static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|(uint32_t)GetBValue(c);}
static inline void px(int x,int y,COLORREF c){if((unsigned)x<(unsigned)g_w&&(unsigned)y<(unsigned)g_h&&g_pixels)g_pixels[(size_t)y*g_w+x]=pack(c);}
static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);r=(std::max)(0,(std::min)(255,r));g=(std::max)(0,(std::min)(255,g));b=(std::max)(0,(std::min)(255,b));return RGB(r,g,b);}
static COLORREF mixc(COLORREF a,COLORREF b,float t){t=(std::max)(0.0f,(std::min)(1.0f,t));return RGB((int)(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),(int)(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),(int)(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));}
static unsigned hash2(int x,int y,unsigned s=0x9e3779b9u){unsigned h=(unsigned)x*0x85ebca6bu^(unsigned)y*0xc2b2ae35u^s;h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;}
static float h01(int x,int y,unsigned s){return (hash2(x,y,s)&65535)/65535.0f;}
static float smooth01(float t){return t*t*(3.0f-2.0f*t);}
static float noise2(float x,float y,unsigned s){int x0=(int)floorf(x),y0=(int)floorf(y);float tx=smooth01(x-x0),ty=smooth01(y-y0);float a=h01(x0,y0,s),b=h01(x0+1,y0,s),c=h01(x0,y0+1,s),d=h01(x0+1,y0+1,s);float u=a+(b-a)*tx,v=c+(d-c)*tx;return u+(v-u)*ty;}
static float fbm(float x,float y,unsigned s){float f=0,a=.55f;for(int i=0;i<5;++i){f+=noise2(x,y,s+i*977u)*a;x*=2.01f;y*=2.03f;a*=.5f;}return f;}

static void destroyBack(){g_pixels=nullptr;if(g_backDC){if(g_backOld)SelectObject(g_backDC,g_backOld);if(g_backBmp)DeleteObject(g_backBmp);DeleteDC(g_backDC);}g_backDC=NULL;g_backBmp=NULL;g_backOld=NULL;g_w=g_h=0;}
static void ensureBack(HWND h,int w,int ht){if(w<=0||ht<=0)return;if(g_backDC&&w==g_w&&ht==g_h)return;destroyBack();HDC wnd=GetDC(h);g_backDC=CreateCompatibleDC(wnd);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-ht;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void*bits=nullptr;g_backBmp=CreateDIBSection(wnd,&bi,DIB_RGB_COLORS,&bits,NULL,0);g_pixels=(uint32_t*)bits;g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);ReleaseDC(h,wnd);g_w=w;g_h=ht;}

static float terrainHeight(float x,float y){float broad=(fbm(x*.032f,y*.032f,11u)-.48f)*1.45f;float detail=(fbm(x*.13f,y*.13f,29u)-.5f)*.32f;float ridge=fabsf(fbm(x*.052f+17,y*.052f-9,71u)-.5f);return broad+detail+(std::max)(0.0f,.17f-ridge)*2.8f;}
static float resourceField(float wx,float wy){float best=0;for(const CrystalNode&n:g_world.getCrystalNodes()){if(n.reserve<=0)continue;float dx=wx-(n.x+.5f),dy=wy-(n.y+.5f),d=sqrtf(dx*dx+dy*dy)/(n.radius*1.18f);if(d>=1)continue;float f=(1-d)*1.18f+(fbm(wx*.34f,wy*.34f,n.seed)-.55f)*.3f;best=(std::max)(best,f);}return (std::max)(0.0f,best);}
static COLORREF terrainColor(float x,float y,float z){float n=fbm(x*.055f,y*.055f,101u),f=fbm(x*.29f,y*.29f,303u);COLORREF c=mixc(RGB(43,57,42),RGB(79,91,57),n);c=shade(c,.84f+f*.20f+z*.028f);float r=resourceField(x,y);if(r>.035f){float t=(std::min)(1.0f,r*1.35f);COLORREF flesh=mixc(RGB(101,65,52),RGB(171,106,79),.30f+.70f*t);c=mixc(c,flesh,(std::min)(.92f,t));}return c;}
static float TW(){return 34.0f*g_zoom;} static float TH(){return 17.0f*g_zoom;} static float ZS(){return 22.0f*g_zoom;}
static POINT worldToScreen(float x,float y,float z,int cx,int cy){float rx=x-g_camX,ry=y-g_camY;return {(LONG)lroundf(cx+(rx-ry)*TW()*.5f),(LONG)lroundf(cy+(rx+ry)*TH()*.5f-z*ZS())};}

static bool ell(float x,float y,float z,float cx,float cy,float cz,float rx,float ry,float rz){float dx=(x-cx)/rx,dy=(y-cy)/ry,dz=(z-cz)/rz;return dx*dx+dy*dy+dz*dz<=1.0f;}
static bool visceroidShape(float x,float y,float z){
    // Compact, low Y-shaped mass: broad central body with three short lobes.
    bool in=ell(x,y,z,0,0,.78f,1.48f,1.38f,.78f);
    in|=ell(x,y,z,-.92f,-.26f,.72f,.82f,.72f,.66f);
    in|=ell(x,y,z, .92f,-.26f,.72f,.82f,.72f,.66f);
    in|=ell(x,y,z,0,.96f,.68f,.72f,.92f,.62f);
    // Shallow central notch only; never cuts the body into a phallic silhouette.
    bool notch=ell(x,y,z,0,-1.18f,.98f,.22f,.28f,.42f);
    return in&&!notch;
}
static void buildSoftBody(){
    g_body.clear();g_springs.clear();
    const int NX=17,NY=17,NZ=9;const float s=.255f;
    std::vector<int> map(NX*NY*NZ,-1);auto idx=[&](int x,int y,int z){return (z*NY+y)*NX+x;};
    for(int iz=0;iz<NZ;++iz)for(int iy=0;iy<NY;++iy)for(int ix=0;ix<NX;++ix){float x=(ix-(NX-1)*.5f)*s,y=(iy-(NY-1)*.5f)*s,z=iz*s+.08f;if(!visceroidShape(x,y,z))continue;unsigned h=hash2(ix*37+iy,iz*19,0xB10Bu);COLORREF col=(h&3u)==0?RGB(82,28,31):((h&3u)==1?RGB(108,38,41):RGB(137,49,52));Particle p;p.p={128+x,128+y,terrainHeight(128+x,128+y)+z+.12f};p.v={0,0,0};p.f={0,0,0};p.mass=.055f;p.radius=.115f;p.restRadius=sqrtf(x*x+y*y+(z-.68f)*(z-.68f));p.c=col;p.contact=false;map[idx(ix,iy,iz)]=(int)g_body.size();g_body.push_back(p);}
    const int o[][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,0},{1,-1,0},{1,0,1},{1,0,-1},{0,1,1},{0,1,-1},{1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1},{2,0,0},{0,2,0},{0,0,2}};
    for(int z=0;z<NZ;++z)for(int y=0;y<NY;++y)for(int x=0;x<NX;++x){int a=map[idx(x,y,z)];if(a<0)continue;for(auto&d:o){int xx=x+d[0],yy=y+d[1],zz=z+d[2];if(xx<0||yy<0||zz<0||xx>=NX||yy>=NY||zz>=NZ)continue;int b=map[idx(xx,yy,zz)];if(b<0)continue;float rest=len(sub(g_body[b].p,g_body[a].p));g_springs.push_back({a,b,rest,rest<.45f?145.0f:84.0f,2.8f});}}
}
static V3 bodyCOM(){V3 c={0,0,0};float m=0;for(const Particle&p:g_body){c=add(c,mul(p.p,p.mass));m+=p.mass;}return m>0?mul(c,1.0f/m):V3{128,128,0};}
static void physicsSubstep(float dt,float ix,float iy){V3 com=bodyCOM();for(Particle&p:g_body){p.f={0,0,-20.0f*p.mass};p.contact=false;float height=(p.p.z-com.z);float drive=.28f+(height>0?1.0f:.0f);p.f.x+=ix*5.2f*drive;p.f.y+=iy*5.2f*drive;}for(const Spring&s:g_springs){V3 d=sub(g_body[s.b].p,g_body[s.a].p);float L=len(d);if(L<1e-5f)continue;V3 n=mul(d,1.0f/L);float rel=dot(sub(g_body[s.b].v,g_body[s.a].v),n);V3 f=mul(n,s.k*(L-s.rest)+s.damp*rel);g_body[s.a].f=add(g_body[s.a].f,f);g_body[s.b].f=sub(g_body[s.b].f,f);}for(Particle&p:g_body){V3 a=mul(p.f,1.0f/p.mass);p.v=add(p.v,mul(a,dt));p.v=mul(p.v,expf(-.12f*dt));p.p=add(p.p,mul(p.v,dt));float gh=terrainHeight(p.p.x,p.p.y)+p.radius;if(p.p.z<gh){p.p.z=gh;p.contact=true;if(p.v.z<0)p.v.z*=-.06f;float fr=expf(-8.5f*dt);p.v.x*=fr;p.v.y*=fr;}p.p.x=(std::max)(4.0f,(std::min)(252.0f,p.p.x));p.p.y=(std::max)(4.0f,(std::min)(252.0f,p.p.y));}}

static bool isCrystal(VoxelMaterial m){return m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;}
static bool findTarget(V3 c){float best=9999;int bx=-1,by=-1;for(int y=(int)c.y-5;y<=(int)c.y+5;++y)for(int x=(int)c.x-5;x<=(int)c.x+5;++x){if(!g_world.isInside(x,y)||!isCrystal(g_world.getCell(x,y).material))continue;float dx=x+.5f-c.x,dy=y+.5f-c.y,d=dx*dx+dy*dy;if(d<best&&d<18){best=d;bx=x;by=y;}}g_targetX=bx;g_targetY=by;return bx>=0;}
static void spawnHarvest(V3 c){if(g_targetX<0)return;for(int i=0;i<7;++i){float a=(hash2(i,(int)(c.x*17+c.y*13),91u)&1023)/1023.0f*6.28318f;HarvestParticle p;p.p={g_targetX+.5f,g_targetY+.5f,terrainHeight(g_targetX+.5f,g_targetY+.5f)+.15f};p.v={cosf(a)*1.2f,sinf(a)*1.2f,.55f};p.life=.75f;g_harvestFx.push_back(p);}}
static void updateHarvest(float dt,V3 c){for(HarvestParticle&p:g_harvestFx){V3 d=sub(c,p.p);float L=len(d)+.001f;V3 n=mul(d,1.0f/L);p.v=add(p.v,mul(n,(15.0f/(.7f+L))*dt));p.v.z-=4.0f*dt;p.v=mul(p.v,expf(-.55f*dt));p.p=add(p.p,mul(p.v,dt));p.life-=dt;}g_harvestFx.erase(std::remove_if(g_harvestFx.begin(),g_harvestFx.end(),[](const HarvestParticle&p){return p.life<=0;}),g_harvestFx.end());}
static void updateGame(float dt){float ix=0,iy=0;if(g_keys['A']||g_keys[VK_LEFT])ix-=1;if(g_keys['D']||g_keys[VK_RIGHT])ix+=1;if(g_keys['W']||g_keys[VK_UP])iy-=1;if(g_keys['S']||g_keys[VK_DOWN])iy+=1;if(ix&&iy){ix*=.7071067f;iy*=.7071067f;}for(int i=0;i<4;++i)physicsSubstep(dt*.25f,ix,iy);static unsigned frame=0;g_world.update(frame++);V3 c=bodyCOM();float follow=1-expf(-5.2f*dt);g_camX+=(c.x-g_camX)*follow;g_camY+=(c.y-g_camY)*follow;if(g_keys['H']){if(g_targetX<0||!isCrystal(g_world.getCell(g_targetX,g_targetY).material))findTarget(c);g_harvestClock+=dt;if(g_targetX>=0&&g_harvestClock>=.11f){g_harvestClock=0;spawnHarvest(c);int got=g_world.harvest(g_targetX,g_targetY,3);g_crystals+=got;if(got)g_biomass+=got/3;if(!got)findTarget(c);}}else{g_targetX=g_targetY=-1;g_harvestClock=0;}updateHarvest(dt,c);}

static void drawTerrain(int cx,int cy){const float step=.72f;int R=(int)ceilf((g_w/TW()+g_h/TH())*.72f)+24;int bx=(int)floorf(g_camX/step),by=(int)floorf(g_camY/step);SelectObject(g_backDC,GetStockObject(DC_BRUSH));SelectObject(g_backDC,GetStockObject(DC_PEN));for(int s=-2*R;s<=2*R;++s)for(int dx=-R;dx<=R;++dx){int dy=s-dx;if(dy<-R||dy>R)continue;float x=(bx+dx)*step,y=(by+dy)*step;float z00=terrainHeight(x,y),z10=terrainHeight(x+step,y),z11=terrainHeight(x+step,y+step),z01=terrainHeight(x,y+step);POINT q[4]={worldToScreen(x,y,z00,cx,cy),worldToScreen(x+step,y,z10,cx,cy),worldToScreen(x+step,y+step,z11,cx,cy),worldToScreen(x,y+step,z01,cx,cy)};float z=(z00+z10+z11+z01)*.25f;COLORREF c=terrainColor(x+.35f,y+.35f,z);SetDCBrushColor(g_backDC,c);SetDCPenColor(g_backDC,c);Polygon(g_backDC,q,4);}}

static void fillDisc(int cx,int cy,int r,COLORREF c){for(int y=-r;y<=r;++y){int span=(int)sqrtf((float)(r*r-y*y));for(int x=-span;x<=span;++x)px(cx+x,cy+y,c);}}
static void drawBody(int cx,int cy){
    std::vector<ScreenParticle> sp;sp.reserve(g_body.size());
    for(const Particle&p:g_body){POINT s=worldToScreen(p.p.x,p.p.y,p.p.z,cx,cy);float h=p.p.z-terrainHeight(p.p.x,p.p.y);float light=(std::max)(0.0f,(std::min)(1.0f,h/1.9f));int rr=(std::max)(3,(int)lroundf(4.6f*g_zoom));sp.push_back({s.x,s.y,rr,p.p.x+p.p.y+p.p.z,light,p.c});}
    std::sort(sp.begin(),sp.end(),[](const ScreenParticle&a,const ScreenParticle&b){return a.depth<b.depth;});
    // First pass: overlapping discs weld all particles into one continuous body.
    for(const ScreenParticle&q:sp){COLORREF c=shade(q.c,.76f+.32f*q.light);fillDisc(q.x,q.y,q.r,c);}
    // Second pass: tiny highlights create wet granular texture without exposing separate particles.
    for(size_t i=0;i<sp.size();i+=7){const ScreenParticle&q=sp[i];fillDisc(q.x-1,q.y-1,(std::max)(1,q.r/4),shade(q.c,1.18f));}
    if(g_evo.hasPart(SPORE_PART_EYE)){V3 c=bodyCOM();POINT e=worldToScreen(c.x+.65f,c.y-.48f,c.z+.32f,cx,cy);fillDisc(e.x,e.y,(std::max)(3,(int)(3.3f*g_zoom)),RGB(202,200,177));fillDisc(e.x+1,e.y,(std::max)(1,(int)(1.15f*g_zoom)),RGB(16,18,16));}
}
static void drawHarvestFx(int cx,int cy){for(const HarvestParticle&p:g_harvestFx){POINT s=worldToScreen(p.p.x,p.p.y,p.p.z,cx,cy);fillDisc(s.x,s.y,(std::max)(1,(int)g_zoom),RGB(188,177,145));}}
static void text(HDC dc,int x,int y,const std::wstring&s){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(230,229,215));TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void render(HWND h){RECT rc;GetClientRect(h,&rc);ensureBack(h,rc.right,rc.bottom);std::fill(g_pixels,g_pixels+(size_t)g_w*g_h,pack(RGB(43,52,41)));int cx=g_w/2,cy=g_h/2+35;drawTerrain(cx,cy);drawHarvestFx(cx,cy);drawBody(cx,cy);if(g_showHud){text(g_backDC,18,18,L"soft-body mass   wheel: zoom   H: absorber   F1: HUD");text(g_backDC,18,40,L"Biomasa "+std::to_wstring(g_biomass)+L"   Cristales "+std::to_wstring(g_crystals));}HDC wnd=GetDC(h);BitBlt(wnd,0,0,g_w,g_h,g_backDC,0,0,SRCCOPY);ReleaseDC(h,wnd);}
static void initGame(){g_world.resize(256,256);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(122,122,10,360,1234u);g_world.addCrystalNode(137,132,12,480,777u);g_world.addCrystalNode(117,139,9,300,991u);g_evo.resetToVisceroid();buildSoftBody();V3 c=bodyCOM();g_camX=c.x;g_camY=c.y;}
static void addUpgrade(SporePartType t,const char*id,int bio,int cry,int vis,int spd,int dmg,int arm){SporePartUpgrade u;u.type=t;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vis;u.speedDelta=spd;u.damageDelta=dmg;u.armorDelta=arm;g_evo.unlock(u,g_biomass,g_crystals);}
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_NCHITTEST:{LRESULT r=DefWindowProcW(h,m,w,l);return r==HTCLIENT?HTCAPTION:r;}case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBack();return 0;case WM_DESTROY:destroyBack();PostQuitMessage(0);return 0;case WM_MOUSEWHEEL:{short d=GET_WHEEL_DELTA_WPARAM(w);g_zoom*=d>0?1.10f:.91f;g_zoom=(std::max)(.65f,(std::min)(2.8f,g_zoom));return 0;}case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w==VK_ESCAPE){DestroyWindow(h);return 0;}if(w==VK_F1){g_showHud=!g_showHud;return 0;}if(w=='1')addUpgrade(SPORE_PART_EYE,"eye",25,10,4,0,0,0);return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)g_keys[i]=false;return 0;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPSoftBodyMass";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE|WS_THICKFRAME,80,60,1440,900,NULL,NULL,inst,NULL);if(!g_hwnd)return 2;ShowWindow(g_hwnd,show);MSG msg;double acc=0,racc=0;const double step=1.0/120.0,renderStep=1.0/60.0;bool run=true;while(run){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.04)dt=.04;acc+=dt;racc+=dt;while(acc>=step){updateGame((float)step);acc-=step;}if(racc>=renderStep){render(g_hwnd);racc=fmod(racc,renderStep);}Sleep(1);}destroyBack();return 0;}
