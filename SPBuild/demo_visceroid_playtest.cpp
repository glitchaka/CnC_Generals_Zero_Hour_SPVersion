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

struct V3{float x,y,z;};
struct Marker{V3 q,v;float pigment,restRad;};
struct FX{V3 p,v;float life;};
struct Food{float x,y;int amount;bool alive;};
struct Rock{float x,y,r;};
struct Hazard{float x,y,r;};

static SporeVoxelWorld world; static SporeEvolutionState evo;
static std::vector<Marker> markers; static std::vector<FX> fx;
static std::vector<Food> foods; static std::vector<Rock> rocks; static std::vector<Hazard> hazards;
static bool keys[256]={},showHud=true;
static int bio=250,cry=100,targetX=-1,targetY=-1,score=0;
static float health=100.0f,stamina=100.0f,harvestClock=0;
static float cxw=128.0f,cyw=128.0f,cvx=0.0f,cvy=0.0f;
static float camX=128.0f,camY=128.0f,zoomv=1.6f,flowRamp=0.0f;
static LARGE_INTEGER freq={},prev={};
static HDC backDC=NULL; static HBITMAP backBmp=NULL,backOld=NULL; static uint32_t* pix=nullptr; static int W=0,H=0;
static HDC terrainDC=NULL; static HBITMAP terrainBmp=NULL,terrainOld=NULL; static uint32_t* terrainPix=nullptr; static int terrainW=0,terrainH=0;
static float terrainCamX=0,terrainCamY=0,terrainZoom=0; static bool terrainDirty=true;
static const int TERRAIN_MARGIN=360;
static std::vector<uint32_t> density,pigment;

static inline V3 add(V3 a,V3 b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
static inline V3 mul(V3 a,float s){return{a.x*s,a.y*s,a.z*s};}
static unsigned hash2(int x,int y,unsigned s=0x9e3779b9u){unsigned h=(unsigned)x*0x85ebca6bu^(unsigned)y*0xc2b2ae35u^s;h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;}
static float h01(int x,int y,unsigned s){return (hash2(x,y,s)&65535)/65535.0f;}
static float sm(float t){return t*t*(3.0f-2.0f*t);}
static float noise2(float x,float y,unsigned s){int x0=(int)floorf(x),y0=(int)floorf(y);float tx=sm(x-x0),ty=sm(y-y0);float a=h01(x0,y0,s),b=h01(x0+1,y0,s),c=h01(x0,y0+1,s),d=h01(x0+1,y0+1,s);float u=a+(b-a)*tx,v=c+(d-c)*tx;return u+(v-u)*ty;}
static float fbm(float x,float y,unsigned s){float f=0,a=.56f;for(int i=0;i<4;++i){f+=noise2(x,y,s+i*977u)*a;x*=2.01f;y*=2.03f;a*=.5f;}return f;}
static COLORREF mixc(COLORREF a,COLORREF b,float t){t=(std::max)(0.f,(std::min)(1.f,t));return RGB((int)(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),(int)(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),(int)(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));}
static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);return RGB((std::max)(0,(std::min)(255,r)),(std::max)(0,(std::min)(255,g)),(std::max)(0,(std::min)(255,b)));}
static inline uint32_t pack(COLORREF c){return 0xff000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|(uint32_t)GetBValue(c);}
static inline void put(int x,int y,COLORREF c){if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H&&pix)pix[(size_t)y*W+x]=pack(c);}
static void disc(int x,int y,int r,COLORREF c){for(int yy=-r;yy<=r;++yy)for(int xx=-r;xx<=r;++xx)if(xx*xx+yy*yy<=r*r)put(x+xx,y+yy,c);}

static void destroyBack(){pix=nullptr;if(backDC){if(backOld)SelectObject(backDC,backOld);if(backBmp)DeleteObject(backBmp);DeleteDC(backDC);}backDC=NULL;backBmp=NULL;backOld=NULL;W=H=0;terrainDirty=true;}
static void ensureBack(HWND h,int w,int hh){if(w<=0||hh<=0)return;if(backDC&&w==W&&hh==H)return;destroyBack();HDC dc=GetDC(h);backDC=CreateCompatibleDC(dc);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-hh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void*b=nullptr;backBmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&b,NULL,0);pix=(uint32_t*)b;backOld=(HBITMAP)SelectObject(backDC,backBmp);ReleaseDC(h,dc);W=w;H=hh;}
static void destroyTerrain(){terrainPix=nullptr;if(terrainDC){if(terrainOld)SelectObject(terrainDC,terrainOld);if(terrainBmp)DeleteObject(terrainBmp);DeleteDC(terrainDC);}terrainDC=NULL;terrainBmp=NULL;terrainOld=NULL;terrainW=terrainH=0;}
static void ensureTerrain(HWND h){int ww=W+2*TERRAIN_MARGIN,hh=H+2*TERRAIN_MARGIN;if(terrainDC&&terrainW==ww&&terrainH==hh)return;destroyTerrain();HDC dc=GetDC(h);terrainDC=CreateCompatibleDC(dc);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=ww;bi.bmiHeader.biHeight=-hh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void*b=nullptr;terrainBmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&b,NULL,0);terrainPix=(uint32_t*)b;terrainOld=(HBITMAP)SelectObject(terrainDC,terrainBmp);ReleaseDC(h,dc);terrainW=ww;terrainH=hh;terrainDirty=true;}

static float ground(float x,float y){return (fbm(x*.028f,y*.028f,11u)-.48f)*1.25f+(fbm(x*.105f,y*.105f,29u)-.5f)*.28f;}
static float dist2(float x,float y,float a,float b){float dx=x-a,dy=y-b;return dx*dx+dy*dy;}
static bool inHazard(float x,float y){for(const Hazard&h:hazards)if(dist2(x,y,h.x,h.y)<h.r*h.r)return true;return false;}
static float resource(float x,float y){float best=0;for(const CrystalNode&n:world.getCrystalNodes()){if(n.reserve<=0)continue;float dx=x-(n.x+.5f),dy=y-(n.y+.5f),d=sqrtf(dx*dx+dy*dy)/(n.radius*1.18f);if(d<1){float f=(1-d)*1.16f+(noise2(x*.35f,y*.35f,n.seed)-.55f)*.28f;best=(std::max)(best,f);}}return (std::max)(0.f,best);}
static COLORREF terrainColor(float x,float y){float z=ground(x,y);float e=.12f;float gx=ground(x+e,y)-ground(x-e,y),gy=ground(x,y+e)-ground(x,y-e);float light=.93f-gx*.22f-gy*.14f;float n=fbm(x*.05f,y*.05f,101u),fine=noise2(x*.3f,y*.3f,303u);COLORREF c=mixc(RGB(40,54,39),RGB(76,91,55),n);c=shade(c,light*(.90f+fine*.16f));float r=resource(x,y);if(r>.04f){float t=(std::min)(1.f,r*1.35f);COLORREF flesh=mixc(RGB(102,66,52),RGB(178,110,82),t);c=mixc(c,flesh,(std::min)(.93f,t));}if(inHazard(x,y))c=mixc(c,RGB(105,49,34),.72f);return c;}
static float TW(float z){return 34.f*z;}static float TH(float z){return 17.f*z;}static float ZS(float z){return 22.f*z;}
static POINT project(float x,float y,float z,int sx,int sy,float ccx,float ccy,float zm){float rx=x-ccx,ry=y-ccy;return{(LONG)lroundf(sx+(rx-ry)*TW(zm)*.5f),(LONG)lroundf(sy+(rx+ry)*TH(zm)*.5f-z*ZS(zm))};}
static POINT worldToScreen(float x,float y,float z,int sx,int sy){return project(x,y,z,sx,sy,camX,camY,zoomv);}
static void screenToWorld(float sx,float sy,int ccx,int ccy,float centerX,float centerY,float zm,float&wx,float&wy){float u=(sx-ccx)/(TW(zm)*.5f),v=(sy-ccy)/(TH(zm)*.5f);wx=centerX+(u+v)*.5f;wy=centerY+(v-u)*.5f;}

static void rebuildTerrain(HWND h){ensureTerrain(h);terrainCamX=camX;terrainCamY=camY;terrainZoom=zoomv;int ccx=terrainW/2,ccy=terrainH/2;for(int y=0;y<terrainH;y+=2)for(int x=0;x<terrainW;x+=2){float wx,wy;screenToWorld((float)x,(float)y,ccx,ccy,terrainCamX,terrainCamY,terrainZoom,wx,wy);uint32_t c=pack(terrainColor(wx,wy));size_t i=(size_t)y*terrainW+x;terrainPix[i]=c;if(x+1<terrainW)terrainPix[i+1]=c;if(y+1<terrainH){terrainPix[i+terrainW]=c;if(x+1<terrainW)terrainPix[i+terrainW+1]=c;}}terrainDirty=false;}
static void blitTerrain(HWND h){ensureTerrain(h);float dx=camX-terrainCamX,dy=camY-terrainCamY;int sx=TERRAIN_MARGIN+(int)lroundf((dx-dy)*TW(zoomv)*.5f),sy=TERRAIN_MARGIN+(int)lroundf((dx+dy)*TH(zoomv)*.5f);bool zoomChanged=fabsf(zoomv-terrainZoom)>.0001f;bool out=sx<60||sy<60||sx+W>terrainW-60||sy+H>terrainH-60;if(terrainDirty||zoomChanged||out){rebuildTerrain(h);sx=TERRAIN_MARGIN;sy=TERRAIN_MARGIN;}BitBlt(backDC,0,0,W,H,terrainDC,sx,sy,SRCCOPY);}

static bool insideShape(float x,float y,float z){float nx=x/1.45f,ny=y/1.25f,nz=(z-.68f)/.72f;float base=nx*nx+ny*ny+nz*nz;float lump=.09f*sinf(y*3.1f)+.06f*cosf(x*4.0f);return base<=1.0f+lump;}
static void buildMarkers(){markers.clear();const float s=.205f;for(int iz=0;iz<9;++iz)for(int iy=-7;iy<=7;++iy)for(int ix=-8;ix<=8;++ix){float x=ix*s,y=iy*s,z=.08f+iz*s;if(!insideShape(x,y,z))continue;float nx=x/1.45f,ny=y/1.25f,nz=(z-.68f)/.72f;unsigned h=hash2(ix*31+iy*7,iz*17,0xB10Bu);Marker m;m.q={x,y,z};m.v={0,0,0};m.pigment=.20f+.80f*((h&255)/255.0f);m.restRad=sqrtf(nx*nx+ny*ny+nz*nz);markers.push_back(m);}density.reserve(300*220);pigment.reserve(300*220);}
static float bodyBaseZ(){return ground(cxw,cyw)+.06f;}
static void resetCreature(){cxw=128;cyw=128;cvx=cvy=0;flowRamp=0;health=100;stamina=100;buildMarkers();camX=cxw;camY=cyw;}

static void resolveRockCollision(){const float bodyR=1.15f;for(const Rock&r:rocks){float dx=cxw-r.x,dy=cyw-r.y,d=sqrtf(dx*dx+dy*dy),minD=bodyR+r.r;if(d<minD&&d>.0001f){float nx=dx/d,ny=dy/d,pen=minD-d;cxw+=nx*pen;cyw+=ny*pen;float vn=cvx*nx+cvy*ny;if(vn<0){cvx-=vn*nx*1.25f;cvy-=vn*ny*1.25f;}cvx*=.62f;cvy*=.62f;}}}
static void collectFood(){for(Food&f:foods)if(f.alive&&dist2(cxw,cyw,f.x,f.y)<1.35f*1.35f){f.alive=false;bio+=f.amount;score+=f.amount;health=(std::min)(100.0f,health+f.amount*.18f);}}
static void updateHazards(float dt){if(inHazard(cxw,cyw)){health-=18.0f*dt;stamina=(std::max)(0.0f,stamina-25.0f*dt);}if(health<=0)resetCreature();}

static void updateFlow(float dt,float ix,float iy,bool boost){float input=sqrtf(ix*ix+iy*iy);if(input>.001f){ix/=input;iy/=input;}flowRamp+=(input-flowRamp)*(1.0f-expf(-6.5f*dt));float speedMul=boost?1.55f:1.0f;float omega=2.55f*flowRamp*speedMul;float rx=1.35f,rz=.66f;float bottomBack=0,weight=0;
for(Marker&m:markers){float u=m.q.x*ix+m.q.y*iy;float z=m.q.z-.68f;float nx0=m.q.x/1.45f,ny0=m.q.y/1.25f,nz0=z/.72f;float rad=sqrtf(nx0*nx0+ny0*ny0+nz0*nz0);float flowWeight=(std::max)(0.0f,(std::min)(1.0f,(rad-.22f)/.55f));V3 desired={0,0,0};if(input>.001f){float du=omega*z*(rx/rz)*flowWeight;float dz=-omega*u*(rz/rx)*flowWeight;desired.x=ix*du;desired.y=iy*du;desired.z=dz;}float k=1.0f-expf(-10.0f*dt);m.v.x+=(desired.x-m.v.x)*k;m.v.y+=(desired.y-m.v.y)*k;m.v.z+=(desired.z-m.v.z)*k;m.q=add(m.q,mul(m.v,dt));
float nx=m.q.x/1.45f,ny=m.q.y/1.25f,nz=(m.q.z-.68f)/.72f;float q=sqrtf(nx*nx+ny*ny+nz*nz);if(q>.0001f){float target=m.restRad;float corr=(target-q)*(1.0f-expf(-3.8f*dt));float scale=(q+corr)/q;m.q.x*=scale;m.q.y*=scale;m.q.z=.68f+(m.q.z-.68f)*scale;}nx=m.q.x/1.45f;ny=m.q.y/1.25f;nz=(m.q.z-.68f)/.72f;q=sqrtf(nx*nx+ny*ny+nz*nz);if(q>1.0f){float inv=.995f/q;m.q.x*=inv;m.q.y*=inv;m.q.z=.68f+(m.q.z-.68f)*inv;}
float wz=bodyBaseZ()+m.q.z,gh=ground(cxw+m.q.x,cyw+m.q.y)+.035f;if(wz<gh){m.q.z+=gh-wz;if(m.v.z<0)m.v.z=0;}if(m.q.z<.22f&&input>.001f){float back=-(m.v.x*ix+m.v.y*iy);if(back>0){bottomBack+=back;weight+=1.0f;}}}
float traction=weight>0?bottomBack/weight:0;float targetVX=ix*traction,targetVY=iy*traction;float tk=1.0f-expf(-7.0f*dt);cvx+=(targetVX-cvx)*tk;cvy+=(targetVY-cvy)*tk;if(input<.001f){float d=expf(-5.5f*dt);cvx*=d;cvy*=d;}cxw+=cvx*dt;cyw+=cvy*dt;cxw=(std::max)(5.f,(std::min)(251.f,cxw));cyw=(std::max)(5.f,(std::min)(251.f,cyw));resolveRockCollision();}

static bool isCrystal(VoxelMaterial m){return m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN;}
static bool findTarget(){float best=9999;int bx=-1,by=-1;for(int y=(int)cyw-5;y<=(int)cyw+5;++y)for(int x=(int)cxw-5;x<=(int)cxw+5;++x){if(!world.isInside(x,y)||!isCrystal(world.getCell(x,y).material))continue;float dx=x+.5f-cxw,dy=y+.5f-cyw,d=dx*dx+dy*dy;if(d<best&&d<18){best=d;bx=x;by=y;}}targetX=bx;targetY=by;return bx>=0;}
static void spawnFX(){if(targetX<0)return;for(int i=0;i<6;++i){float a=(hash2(i,(int)(cxw*19+cyw*13),91u)&1023)/1023.f*6.28318f;fx.push_back({{targetX+.5f,targetY+.5f,ground(targetX+.5f,targetY+.5f)+.15f},{cosf(a)*1.1f,sinf(a)*1.1f,.55f},.7f});}}
static void updateFX(float dt){V3 target={cxw,cyw,bodyBaseZ()+.65f};for(FX&p:fx){V3 d={target.x-p.p.x,target.y-p.p.y,target.z-p.p.z};float L=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z)+.001f;p.v.x+=d.x/L*(13.f/(.6f+L))*dt;p.v.y+=d.y/L*(13.f/(.6f+L))*dt;p.v.z+=d.z/L*(13.f/(.6f+L))*dt;p.v=mul(p.v,expf(-.6f*dt));p.p=add(p.p,mul(p.v,dt));p.life-=dt;}fx.erase(std::remove_if(fx.begin(),fx.end(),[](const FX&p){return p.life<=0;}),fx.end());}
static void update(float dt){float ix=0,iy=0;if(keys['A']||keys[VK_LEFT])ix-=1;if(keys['D']||keys[VK_RIGHT])ix+=1;if(keys['W']||keys[VK_UP])iy-=1;if(keys['S']||keys[VK_DOWN])iy+=1;if(ix&&iy){ix*=.7071067f;iy*=.7071067f;}bool boost=(keys[VK_SHIFT]&&stamina>1.0f&&(ix||iy));if(boost)stamina=(std::max)(0.0f,stamina-24.0f*dt);else stamina=(std::min)(100.0f,stamina+15.0f*dt);for(int i=0;i<3;++i)updateFlow(dt/3.0f,ix,iy,boost);collectFood();updateHazards(dt);static unsigned frame=0;world.update(frame++);float follow=1.0f-expf(-5.0f*dt);camX+=(cxw-camX)*follow;camY+=(cyw-camY)*follow;if(keys['H']){if(targetX<0||!isCrystal(world.getCell(targetX,targetY).material))findTarget();harvestClock+=dt;if(targetX>=0&&harvestClock>.11f){harvestClock=0;spawnFX();int got=world.harvest(targetX,targetY,3);cry+=got;if(got){bio+=got/3;score+=got;}terrainDirty=true;if(!got)findTarget();}}else{targetX=targetY=-1;harvestClock=0;}updateFX(dt);}

static void splatField(int fx0,int fy0,int fw,int fh,int x,int y,int r,float pig){int lx=x-fx0,ly=y-fy0;if(lx<-r||ly<-r||lx>=fw+r||ly>=fh+r)return;int rr=r*r;for(int yy=-r;yy<=r;++yy){int py=ly+yy;if((unsigned)py>=(unsigned)fh)continue;for(int xx=-r;xx<=r;++xx){int px0=lx+xx;if((unsigned)px0>=(unsigned)fw)continue;int d2=xx*xx+yy*yy;if(d2>rr)continue;uint32_t w=(uint32_t)((rr-d2)+1);size_t i=(size_t)py*fw+px0;density[i]+=w;pigment[i]+=(uint32_t)(w*pig*256.0f);}}}
static void drawMass(int sx,int sy){int minx=W,maxx=0,miny=H,maxy=0;std::vector<POINT> pts;pts.reserve(markers.size());float bz=bodyBaseZ();for(const Marker&m:markers){POINT p=worldToScreen(cxw+m.q.x,cyw+m.q.y,bz+m.q.z,sx,sy);pts.push_back(p);minx=(std::min)(minx,(int)p.x);maxx=(std::max)(maxx,(int)p.x);miny=(std::min)(miny,(int)p.y);maxy=(std::max)(maxy,(int)p.y);}int pad=(std::max)(12,(int)(11*zoomv));minx=(std::max)(0,minx-pad);miny=(std::max)(0,miny-pad);maxx=(std::min)(W-1,maxx+pad);maxy=(std::min)(H-1,maxy+pad);int fw=maxx-minx+1,fh=maxy-miny+1;if(fw<=0||fh<=0)return;density.assign((size_t)fw*fh,0);pigment.assign((size_t)fw*fh,0);int r=(std::max)(5,(int)(7.0f*zoomv));for(size_t i=0;i<markers.size();++i)splatField(minx,miny,fw,fh,pts[i].x,pts[i].y,r,markers[i].pigment);uint32_t threshold=(uint32_t)(r*r*.88f);for(int y=1;y<fh-1;++y)for(int x=1;x<fw-1;++x){size_t i=(size_t)y*fw+x;uint32_t d=density[i];if(d<threshold)continue;int gx=(int)density[i+1]-(int)density[i-1],gy=(int)density[i+fw]-(int)density[i-fw];float nl=sqrtf((float)gx*gx+(float)gy*gy)+1.0f;float light=.92f+(-gx/nl)*.13f+(-gy/nl)*.19f;float p=(float)pigment[i]/((float)d*256.0f+1.0f);COLORREF base=mixc(RGB(70,22,27),RGB(151,52,57),.25f+.65f*p);float edge=(std::min)(1.0f,(d-threshold)/(float)(threshold*1.7f));base=shade(base,light*(.86f+.14f*edge));put(minx+x,miny+y,base);}}
static void drawFX(int sx,int sy){for(const FX&p:fx){POINT q=worldToScreen(p.p.x,p.p.y,p.p.z,sx,sy);disc(q.x,q.y,1,RGB(188,177,145));}}
static void drawObjects(int sx,int sy){for(const Food&f:foods)if(f.alive){POINT p=worldToScreen(f.x,f.y,ground(f.x,f.y)+.08f,sx,sy);disc(p.x,p.y,(std::max)(2,(int)(3*zoomv)),RGB(219,155,91));disc(p.x-1,p.y-1,1,RGB(244,203,137));}for(const Rock&r:rocks){POINT p=worldToScreen(r.x,r.y,ground(r.x,r.y)+.08f,sx,sy);int rr=(std::max)(4,(int)(r.r*7.0f*zoomv));disc(p.x,p.y,rr,RGB(61,61,54));disc(p.x-rr/4,p.y-rr/4,(std::max)(2,rr/3),RGB(83,82,71));}}
static void text(HDC dc,int x,int y,const std::wstring&s){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(228,228,214));TextOutW(dc,x,y,s.c_str(),(int)s.size());}
static void render(HWND h){RECT rc;GetClientRect(h,&rc);ensureBack(h,rc.right,rc.bottom);blitTerrain(h);int sx=W/2,sy=H/2+30;drawObjects(sx,sy);drawFX(sx,sy);drawMass(sx,sy);if(showHud){text(backDC,18,18,L"WASD mover  Shift acelerar  H absorber  R reiniciar  rueda zoom  F1 HUD");text(backDC,18,40,L"Vida "+std::to_wstring((int)health)+L"   Aguante "+std::to_wstring((int)stamina)+L"   Biomasa "+std::to_wstring(bio)+L"   Cristales "+std::to_wstring(cry)+L"   Puntaje "+std::to_wstring(score));}HDC dc=GetDC(h);BitBlt(dc,0,0,W,H,backDC,0,0,SRCCOPY);ReleaseDC(h,dc);}

static void initWorldObjects(){foods={{131,126,18,true},{134,130,22,true},{124,135,25,true},{141,139,30,true},{113,126,35,true},{145,120,40,true}};rocks={{132,133,1.15f},{126,137,1.35f},{138,125,1.0f},{118,130,1.45f},{143,135,1.2f}};hazards={{136,137,3.2f},{119,123,2.8f}};terrainDirty=true;}
static void init(){world.resize(256,256);world.generate(0xC0FFEEu);world.addCrystalNode(122,122,10,360,1234u);world.addCrystalNode(137,132,12,480,777u);world.addCrystalNode(117,139,9,300,991u);evo.resetToVisceroid();buildMarkers();initWorldObjects();camX=cxw;camY=cyw;fx.reserve(128);}
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBack();destroyTerrain();return 0;case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(h,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;case WM_MOUSEWHEEL:{short d=GET_WHEEL_DELTA_WPARAM(w);zoomv*=d>0?1.10f:.91f;zoomv=(std::max)(.65f,(std::min)(2.8f,zoomv));terrainDirty=true;return 0;}case WM_KEYDOWN:if(w<256)keys[w]=true;if(w==VK_ESCAPE){DestroyWindow(h);return 0;}if(w==VK_F1){showHud=!showHud;return 0;}if(w=='R'){resetCreature();return 0;}return 0;case WM_KEYUP:if(w<256)keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)keys[i]=false;return 0;case WM_DESTROY:destroyTerrain();destroyBack();PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&prev);init();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPVisceroidPlaytest";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;HWND h=CreateWindowExW(0,wc.lpszClassName,L"SPVersion",WS_POPUP|WS_VISIBLE,80,60,1440,900,NULL,NULL,inst,NULL);if(!h)return 2;ShowWindow(h,show);MSG msg;double acc=0,racc=0;const double step=1.0/120.0,renderStep=1.0/60.0;bool run=true;while(run){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-prev.QuadPart)/(double)freq.QuadPart;prev=now;if(dt>.04)dt=.04;acc+=dt;racc+=dt;while(acc>=step){update((float)step);acc-=step;}if(racc>=renderStep){render(h);racc=fmod(racc,renderStep);}Sleep(1);}destroyTerrain();destroyBack();return 0;}
