#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <math.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <string>
#include "cc0_texture.h"
#include "SporePrototype/SporeVoxelWorld.h"
using namespace SporePrototype;

struct V3{float x,y,z;};
struct Marker{V3 q,v;float pigment,restRad;};
struct FX{V3 p,v;float life;};

static SporeVoxelWorld world;
static std::vector<Marker> markers;
static std::vector<FX> fx;
static bool keys[256]={},showHud=true;
static int bio=250,cry=100,targetX=-1,targetY=-1;
static float harvestClock=0;
static float cxw=128,cyw=128,cvx=0,cvy=0,camX=128,camY=128,zoomv=1.6f,flowRamp=0;
static LARGE_INTEGER freq={},prev={};
static HDC backDC=NULL; static HBITMAP backBmp=NULL,backOld=NULL; static uint32_t* pix=nullptr; static int W=0,H=0;
static HDC terrainDC=NULL; static HBITMAP terrainBmp=NULL,terrainOld=NULL; static uint32_t* terrainPix=nullptr; static int terrainW=0,terrainH=0;
static float terrainCamX=0,terrainCamY=0,terrainZoom=0; static bool terrainDirty=true;
static const int TERRAIN_MARGIN=360;
static std::vector<uint32_t> density,pigment;
static CC0Texture texGround,texRocky,texMud;
static bool assetsReady=false;
static ULONG_PTR gdipToken=0;

static inline V3 add(V3 a,V3 b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
static inline V3 mul(V3 a,float s){return{a.x*s,a.y*s,a.z*s};}
static unsigned hash2(int x,int y,unsigned s=0x9e3779b9u){unsigned h=(unsigned)x*0x85ebca6bu^(unsigned)y*0xc2b2ae35u^s;h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;}
static float h01(int x,int y,unsigned s){return (hash2(x,y,s)&65535)/65535.0f;}
static float sm(float t){return t*t*(3-2*t);}
static float noise2(float x,float y,unsigned s){int x0=(int)floorf(x),y0=(int)floorf(y);float tx=sm(x-x0),ty=sm(y-y0);float a=h01(x0,y0,s),b=h01(x0+1,y0,s),c=h01(x0,y0+1,s),d=h01(x0+1,y0+1,s);float u=a+(b-a)*tx,v=c+(d-c)*tx;return u+(v-u)*ty;}
static float fbm(float x,float y,unsigned s){float f=0,a=.56f;for(int i=0;i<4;++i){f+=noise2(x,y,s+i*977u)*a;x*=2.01f;y*=2.03f;a*=.5f;}return f;}
static COLORREF mixc(COLORREF a,COLORREF b,float t){t=(std::max)(0.f,(std::min)(1.f,t));return RGB((int)(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),(int)(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),(int)(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));}
static COLORREF shade(COLORREF c,float k){return RGB((std::max)(0,(std::min)(255,(int)(GetRValue(c)*k))),(std::max)(0,(std::min)(255,(int)(GetGValue(c)*k))),(std::max)(0,(std::min)(255,(int)(GetBValue(c)*k))));}
static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|GetBValue(c);}
static inline void put(int x,int y,COLORREF c){if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H&&pix)pix[(size_t)y*W+x]=pack(c);}
static void disc(int x,int y,int r,COLORREF c){for(int yy=-r;yy<=r;++yy)for(int xx=-r;xx<=r;++xx)if(xx*xx+yy*yy<=r*r)put(x+xx,y+yy,c);}

static void destroyBack(){pix=nullptr;if(backDC){if(backOld)SelectObject(backDC,backOld);if(backBmp)DeleteObject(backBmp);DeleteDC(backDC);}backDC=NULL;backBmp=NULL;backOld=NULL;W=H=0;terrainDirty=true;}
static void ensureBack(HWND h,int w,int hh){if(w<=0||hh<=0)return;if(backDC&&w==W&&hh==H)return;destroyBack();HDC dc=GetDC(h);backDC=CreateCompatibleDC(dc);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-hh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void*b=nullptr;backBmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&b,NULL,0);pix=(uint32_t*)b;backOld=(HBITMAP)SelectObject(backDC,backBmp);ReleaseDC(h,dc);W=w;H=hh;}
static void destroyTerrain(){terrainPix=nullptr;if(terrainDC){if(terrainOld)SelectObject(terrainDC,terrainOld);if(terrainBmp)DeleteObject(terrainBmp);DeleteDC(terrainDC);}terrainDC=NULL;terrainBmp=NULL;terrainOld=NULL;terrainW=terrainH=0;}
static void ensureTerrain(HWND h){int ww=W+2*TERRAIN_MARGIN,hh=H+2*TERRAIN_MARGIN;if(terrainDC&&terrainW==ww&&terrainH==hh)return;destroyTerrain();HDC dc=GetDC(h);terrainDC=CreateCompatibleDC(dc);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=ww;bi.bmiHeader.biHeight=-hh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void*b=nullptr;terrainBmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&b,NULL,0);terrainPix=(uint32_t*)b;terrainOld=(HBITMAP)SelectObject(terrainDC,terrainBmp);ReleaseDC(h,dc);terrainW=ww;terrainH=hh;terrainDirty=true;}

static float ground(float x,float y){return (fbm(x*.028f,y*.028f,11u)-.48f)*1.25f+(fbm(x*.105f,y*.105f,29u)-.5f)*.28f;}
static float resource(float x,float y){float best=0;for(const CrystalNode&n:world.getCrystalNodes()){if(n.reserve<=0)continue;float dx=x-(n.x+.5f),dy=y-(n.y+.5f),d=sqrtf(dx*dx+dy*dy)/(n.radius*1.18f);if(d<1)best=(std::max)(best,(1-d)*1.16f+(noise2(x*.35f,y*.35f,n.seed)-.55f)*.28f);}return (std::max)(0.f,best);}
static COLORREF terrainColor(float x,float y){
    if(!assetsReady) return (((int)floorf(x)+(int)floorf(y))&1)?RGB(170,0,170):RGB(35,0,35);
    const float scale=.085f;
    COLORREF groundC=cc0Sample(texGround,x*scale,y*scale);
    COLORREF rockyC=cc0Sample(texRocky,x*scale*.82f+0.31f,y*scale*.82f+0.17f);
    COLORREF mudC=cc0Sample(texMud,x*scale*1.15f+0.63f,y*scale*1.15f+0.42f);
    float biome=fbm(x*.035f,y*.035f,505u);
    float wet=fbm(x*.05f+23,y*.05f-11,707u);
    COLORREF c=mixc(groundC,rockyC,(std::max)(0.f,(std::min)(1.f,(biome-.44f)*2.3f))));
    c=mixc(c,mudC,(std::max)(0.f,(std::min)(.68f,(wet-.57f)*2.1f))));
    float e=.12f;float gx=ground(x+e,y)-ground(x-e,y),gy=ground(x,y+e)-ground(x,y-e);c=shade(c,.94f-gx*.16f-gy*.10f);
    float r=resource(x,y);if(r>.05f){float t=(std::min)(1.f,r*1.25f);c=mixc(c,RGB(154,103,72),t*.56f);}return c;
}
static float TW(float z){return 34*z;}static float TH(float z){return 17*z;}static float ZS(float z){return 22*z;}
static POINT project(float x,float y,float z,int sx,int sy,float ccx,float ccy,float zm){float rx=x-ccx,ry=y-ccy;return{(LONG)lroundf(sx+(rx-ry)*TW(zm)*.5f),(LONG)lroundf(sy+(rx+ry)*TH(zm)*.5f-z*ZS(zm))};}
static POINT worldToScreen(float x,float y,float z,int sx,int sy){return project(x,y,z,sx,sy,camX,camY,zoomv);}
static void screenToWorld(float sx,float sy,int ccx,int ccy,float centerX,float centerY,float zm,float&wx,float&wy){float u=(sx-ccx)/(TW(zm)*.5f),v=(sy-ccy)/(TH(zm)*.5f);wx=centerX+(u+v)*.5f;wy=centerY+(v-u)*.5f;}

static void rebuildTerrain(HWND h){ensureTerrain(h);terrainCamX=camX;terrainCamY=camY;terrainZoom=zoomv;int ccx=terrainW/2,ccy=terrainH/2;for(int y=0;y<terrainH;y+=2)for(int x=0;x<terrainW;x+=2){float wx,wy;screenToWorld((float)x,(float)y,ccx,ccy,terrainCamX,terrainCamY,terrainZoom,wx,wy);uint32_t c=pack(terrainColor(wx,wy));size_t i=(size_t)y*terrainW+x;terrainPix[i]=c;if(x+1<terrainW)terrainPix[i+1]=c;if(y+1<terrainH){terrainPix[i+terrainW]=c;if(x+1<terrainW)terrainPix[i+terrainW+1]=c;}}terrainDirty=false;}
static void blitTerrain(HWND h){ensureTerrain(h);float dx=camX-terrainCamX,dy=camY-terrainCamY;int sx=TERRAIN_MARGIN+(int)lroundf((dx-dy)*TW(zoomv)*.5f),sy=TERRAIN_MARGIN+(int)lroundf((dx+dy)*TH(zoomv)*.5f);bool zc=fabsf(zoomv-terrainZoom)>.0001f;bool out=sx<60||sy<60||sx+W>terrainW-60||sy+H>terrainH-60;if(terrainDirty||zc||out){rebuildTerrain(h);sx=TERRAIN_MARGIN;sy=TERRAIN_MARGIN;}BitBlt(backDC,0,0,W,H,terrainDC,sx,sy,SRCCOPY);}

static bool insideShape(float x,float y,float z){float nx=x/1.45f,ny=y/1.25f,nz=(z-.68f)/.72f;float base=nx*nx+ny*ny+nz*nz;float lump=.09f*sinf(y*3.1f)+.06f*cosf(x*4.0f);return base<=1.0f+lump;}
static void buildMarkers(){markers.clear();const float s=.205f;for(int iz=0;iz<9;++iz)for(int iy=-7;iy<=7;++iy)for(int ix=-8;ix<=8;++ix){float x=ix*s,y=iy*s,z=.08f+iz*s;if(!insideShape(x,y,z))continue;float nx=x/1.45f,ny=y/1.25f,nz=(z-.68f)/.72f;unsigned h=hash2(ix*31+iy*7,iz*17,0xB10Bu);markers.push_back({{x,y,z},{0,0,0},.2f+.8f*((h&255)/255.f),sqrtf(nx*nx+ny*ny+nz*nz)});}density.reserve(300*220);pigment.reserve(300*220);}
static float bodyBaseZ(){return ground(cxw,cyw)+.06f;}
static void updateFlow(float dt,float ix,float iy,bool boost){float input=sqrtf(ix*ix+iy*iy);if(input>.001f){ix/=input;iy/=input;}flowRamp+=(input-flowRamp)*(1-expf(-6.5f*dt));float omega=2.55f*flowRamp*(boost?1.55f:1.f);float bottomBack=0,weight=0;for(Marker&m:markers){float u=m.q.x*ix+m.q.y*iy,z=m.q.z-.68f;float nx0=m.q.x/1.45f,ny0=m.q.y/1.25f,nz0=z/.72f;float rad=sqrtf(nx0*nx0+ny0*ny0+nz0*nz0);float fw=(std::max)(0.f,(std::min)(1.f,(rad-.22f)/.55f));V3 des={0,0,0};if(input>.001f){float du=omega*z*(1.35f/.66f)*fw,dz=-omega*u*(.66f/1.35f)*fw;des={ix*du,iy*du,dz};}float k=1-expf(-10*dt);m.v.x+=(des.x-m.v.x)*k;m.v.y+=(des.y-m.v.y)*k;m.v.z+=(des.z-m.v.z)*k;m.q=add(m.q,mul(m.v,dt));float nx=m.q.x/1.45f,ny=m.q.y/1.25f,nz=(m.q.z-.68f)/.72f,q=sqrtf(nx*nx+ny*ny+nz*nz);if(q>.0001f){float corr=(m.restRad-q)*(1-expf(-3.8f*dt));float sc=(q+corr)/q;m.q.x*=sc;m.q.y*=sc;m.q.z=.68f+(m.q.z-.68f)*sc;}float wz=bodyBaseZ()+m.q.z,gh=ground(cxw+m.q.x,cyw+m.q.y)+.035f;if(wz<gh){m.q.z+=gh-wz;if(m.v.z<0)m.v.z=0;}if(m.q.z<.22f&&input>.001f){float back=-(m.v.x*ix+m.v.y*iy);if(back>0){bottomBack+=back;weight++;}}}float traction=weight?bottomBack/weight:0;float tk=1-expf(-7*dt);cvx+=(ix*traction-cvx)*tk;cvy+=(iy*traction-cvy)*tk;if(input<.001f){float d=expf(-5.5f*dt);cvx*=d;cvy*=d;}cxw+=cvx*dt;cyw+=cvy*dt;}
static bool isCrystal(VoxelMaterial m){return m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;}
static bool findTarget(){float best=9999;targetX=targetY=-1;for(int y=(int)cyw-5;y<=(int)cyw+5;++y)for(int x=(int)cxw-5;x<=(int)cxw+5;++x){if(!world.isInside(x,y)||!isCrystal(world.getCell(x,y).material))continue;float dx=x+.5f-cxw,dy=y+.5f-cyw,d=dx*dx+dy*dy;if(d<best&&d<18){best=d;targetX=x;targetY=y;}}return targetX>=0;}
static void update(float dt){float ix=0,iy=0;if(keys['A']||keys[VK_LEFT])ix--;if(keys['D']||keys[VK_RIGHT])ix++;if(keys['W']||keys[VK_UP])iy--;if(keys['S']||keys[VK_DOWN])iy++;if(ix&&iy){ix*=.7071067f;iy*=.7071067f;}for(int i=0;i<3;++i)updateFlow(dt/3,ix,iy,keys[VK_SHIFT]);static unsigned frame=0;world.update(frame++);float f=1-expf(-5*dt);camX+=(cxw-camX)*f;camY+=(cyw-camY)*f;if(keys['H']){if(targetX<0||!isCrystal(world.getCell(targetX,targetY).material))findTarget();harvestClock+=dt;if(targetX>=0&&harvestClock>.11f){harvestClock=0;int got=world.harvest(targetX,targetY,3);cry+=got;bio+=got/3;terrainDirty=true;if(!got)findTarget();}}else{targetX=targetY=-1;harvestClock=0;}}

static void splat(int ox,int oy,int fw,int fh,int x,int y,int r,float pg){int lx=x-ox,ly=y-oy,rr=r*r;for(int yy=-r;yy<=r;++yy){int py=ly+yy;if((unsigned)py>=(unsigned)fh)continue;for(int xx=-r;xx<=r;++xx){int px=lx+xx;if((unsigned)px>=(unsigned)fw)continue;int d2=xx*xx+yy*yy;if(d2>rr)continue;uint32_t w=(uint32_t)(rr-d2+1);size_t i=(size_t)py*fw+px;density[i]+=w;pigment[i]+=(uint32_t)(w*pg*256);}}}
static void drawMass(int sx,int sy){int minx=W,maxx=0,miny=H,maxy=0;std::vector<POINT> pts;pts.reserve(markers.size());float bz=bodyBaseZ();for(const Marker&m:markers){POINT p=worldToScreen(cxw+m.q.x,cyw+m.q.y,bz+m.q.z,sx,sy);pts.push_back(p);minx=(std::min)(minx,(int)p.x);maxx=(std::max)(maxx,(int)p.x);miny=(std::min)(miny,(int)p.y);maxy=(std::max)(maxy,(int)p.y);}int pad=(std::max)(12,(int)(11*zoomv));minx=(std::max)(0,minx-pad);miny=(std::max)(0,miny-pad);maxx=(std::min)(W-1,maxx+pad);maxy=(std::min)(H-1,maxy+pad);int fw=maxx-minx+1,fh=maxy-miny+1;if(fw<=0||fh<=0)return;density.assign((size_t)fw*fh,0);pigment.assign((size_t)fw*fh,0);int r=(std::max)(5,(int)(7*zoomv));for(size_t i=0;i<markers.size();++i)splat(minx,miny,fw,fh,pts[i].x,pts[i].y,r,markers[i].pigment);uint32_t th=(uint32_t)(r*r*.88f);for(int y=1;y<fh-1;++y)for(int x=1;x<fw-1;++x){size_t i=(size_t)y*fw+x;uint32_t d=density[i];if(d<th)continue;int gx=(int)density[i+1]-(int)density[i-1],gy=(int)density[i+fw]-(int)density[i-fw];float nl=sqrtf((float)gx*gx+(float)gy*gy)+1;float light=.92f+(-gx/nl)*.13f+(-gy/nl)*.19f;float p=(float)pigment[i]/((float)d*256+1);put(minx+x,miny+y,shade(mixc(RGB(66,20,25),RGB(148,48,54),.25f+.65f*p),light));}}
static void text(HDC dc,int x,int y,const std::wstring&s,COLORREF c=RGB(228,228,214)){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,c);TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void render(HWND h){RECT rc;GetClientRect(h,&rc);ensureBack(h,rc.right,rc.bottom);blitTerrain(h);drawMass(W/2,H/2+30);if(showHud){text(backDC,18,18,L"WASD mover  Shift acelerar  H absorber  rueda zoom  F1 HUD");text(backDC,18,40,L"Biomasa "+std::to_wstring(bio)+L"   Cristales "+std::to_wstring(cry));if(assetsReady)text(backDC,18,62,L"CC0 REAL: forest_ground_04 + rocky_terrain_02 + mud_forest",RGB(210,230,190));else text(backDC,18,62,L"ERROR: NO SE CARGARON LAS TEXTURAS CC0",RGB(255,90,90));}HDC dc=GetDC(h);BitBlt(dc,0,0,W,H,backDC,0,0,SRCCOPY);ReleaseDC(h,dc);}

static void loadAssets(){std::wstring a=cc0FindAsset(L"forest_ground_04"),b=cc0FindAsset(L"rocky_terrain_02"),c=cc0FindAsset(L"mud_forest");assetsReady=cc0Load(a,texGround)&&cc0Load(b,texRocky)&&cc0Load(c,texMud);}
static void init(){world.resize(256,256);world.generate(0xC0FFEEu);world.addCrystalNode(122,122,10,360,1234u);world.addCrystalNode(137,132,12,480,777u);world.addCrystalNode(117,139,9,300,991u);buildMarkers();loadAssets();camX=cxw;camY=cyw;}
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBack();destroyTerrain();return 0;case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(h,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;case WM_MOUSEWHEEL:{short d=GET_WHEEL_DELTA_WPARAM(w);zoomv*=d>0?1.10f:.91f;zoomv=(std::max)(.65f,(std::min)(2.8f,zoomv));terrainDirty=true;return 0;}case WM_KEYDOWN:if(w<256)keys[w]=true;if(w==VK_ESCAPE){DestroyWindow(h);return 0;}if(w==VK_F1){showHud=!showHud;return 0;}return 0;case WM_KEYUP:if(w<256)keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)keys[i]=false;return 0;case WM_DESTROY:destroyTerrain();destroyBack();PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){Gdiplus::GdiplusStartupInput gi;Gdiplus::GdiplusStartup(&gdipToken,&gi,nullptr);QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&prev);init();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPCC0Textured";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;HWND h=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,80,60,1440,900,NULL,NULL,inst,NULL);if(!h)return 2;ShowWindow(h,show);MSG msg;double acc=0,racc=0;const double step=1.0/120.0,renderStep=1.0/60.0;bool run=true;while(run){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-prev.QuadPart)/(double)freq.QuadPart;prev=now;if(dt>.04)dt=.04;acc+=dt;racc+=dt;while(acc>=step){update((float)step);acc-=step;}if(racc>=renderStep){render(h);racc=fmod(racc,renderStep);}Sleep(1);}destroyTerrain();destroyBack();Gdiplus::GdiplusShutdown(gdipToken);return 0;}
