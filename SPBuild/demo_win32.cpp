#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
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
static float g_anim=0.0f;
static float g_camX=40.0f,g_camY=28.0f;
static HWND g_hwnd=NULL;
static HDC g_backDC=NULL;
static HBITMAP g_backBmp=NULL;
static HBITMAP g_backOld=NULL;
static int g_backW=0,g_backH=0;

static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k);r=(std::max)(0,(std::min)(255,r));g=(std::max)(0,(std::min)(255,g));b=(std::max)(0,(std::min)(255,b));return RGB(r,g,b);}
static COLORREF baseColor(VoxelMaterial m){switch(m){case VOXEL_SOIL:return RGB(92,73,50);case VOXEL_GRASS:return RGB(67,112,55);case VOXEL_ROCK:return RGB(90,94,91);case VOXEL_PEACH_BIOMASS:return RGB(224,143,108);case VOXEL_CRYSTAL_CYAN:return RGB(41,210,227);case VOXEL_CRYSTAL_MAGENTA:return RGB(199,55,165);case VOXEL_CRYSTAL_GREEN:return RGB(91,202,92);case VOXEL_WATER:return RGB(53,105,158);default:return RGB(35,40,37);}}
static void setBrush(HDC dc,COLORREF c){SelectObject(dc,GetStockObject(DC_BRUSH));SetDCBrushColor(dc,c);}
static void setPen(HDC dc,COLORREF c){SelectObject(dc,GetStockObject(DC_PEN));SetDCPenColor(dc,c);}
static void polyFill(HDC dc,POINT*p,int n,COLORREF c){setBrush(dc,c);setPen(dc,c);Polygon(dc,p,n);}
static void fillSolid(HDC dc,const RECT&r,COLORREF c){setBrush(dc,c);FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH));}

static void destroyBackBuffer(){if(g_backDC){if(g_backOld)SelectObject(g_backDC,g_backOld);g_backOld=NULL;if(g_backBmp)DeleteObject(g_backBmp);g_backBmp=NULL;DeleteDC(g_backDC);g_backDC=NULL;}g_backW=g_backH=0;}
static void ensureBackBuffer(HWND h,int w,int ht){if(w<=0||ht<=0)return;if(g_backDC&&w==g_backW&&ht==g_backH)return;destroyBackBuffer();HDC wnd=GetDC(h);g_backDC=CreateCompatibleDC(wnd);g_backBmp=CreateCompatibleBitmap(wnd,w,ht);g_backOld=(HBITMAP)SelectObject(g_backDC,g_backBmp);ReleaseDC(h,wnd);g_backW=w;g_backH=ht;SelectObject(g_backDC,GetStockObject(DC_BRUSH));SelectObject(g_backDC,GetStockObject(DC_PEN));}

static POINT isoWorld(float wx,float wy,float z,int centerX,int centerY){const float TW=38.0f,TH=19.0f;float rx=wx-g_camX,ry=wy-g_camY;POINT p={(LONG)lroundf(centerX+(rx-ry)*(TW*.5f)),(LONG)lroundf(centerY+(rx+ry)*(TH*.5f)-z)};return p;}

static void drawGroundTile(HDC dc,int wx,int wy,COLORREF c,int centerX,int centerY){POINT a=isoWorld((float)wx,(float)wy,0,centerX,centerY),b=isoWorld((float)wx+1,(float)wy,0,centerX,centerY),d=isoWorld((float)wx,(float)wy+1,0,centerX,centerY),e=isoWorld((float)wx+1,(float)wy+1,0,centerX,centerY);POINT q[4]={a,b,e,d};polyFill(dc,q,4,c);}

static void drawMicroCube(HDC dc,int sx,int sy,int z,COLORREF c,int s=3){int hw=s,hh=(std::max)(1,s/2),vh=(std::max)(2,s-1);sy-=z*2;POINT top[4]={{sx,sy-hh},{sx+hw,sy},{sx,sy+hh},{sx-hw,sy}};POINT left[4]={{sx-hw,sy},{sx,sy+hh},{sx,sy+hh+vh},{sx-hw,sy+vh}};POINT right[4]={{sx+hw,sy},{sx,sy+hh},{sx,sy+hh+vh},{sx+hw,sy+vh}};polyFill(dc,left,4,shade(c,.72f));polyFill(dc,right,4,shade(c,.88f));polyFill(dc,top,4,shade(c,1.08f));}

static bool ellipsoid(float x,float y,float z,float cx,float cy,float cz,float rx,float ry,float rz){float dx=(x-cx)/rx,dy=(y-cy)/ry,dz=(z-cz)/rz;return dx*dx+dy*dy+dz*dz<=1.0f;}

struct MV{int x,y,z;COLORREF c;};
static void buildVisceroid(std::vector<MV>& out){out.clear();float pulse=sinf(g_anim)*.45f;for(int z=0;z<=8;++z){for(int y=-11;y<=11;++y){for(int x=-14;x<=14;++x){bool body=false;body|=ellipsoid((float)x,(float)y,(float)z,-5.5f,-1.0f,3.0f,7.5f,6.0f,4.0f+pulse);body|=ellipsoid((float)x,(float)y,(float)z, 5.5f,-1.0f,3.0f,7.5f,6.0f,4.0f-pulse*.4f);body|=ellipsoid((float)x,(float)y,(float)z, 0.0f, 6.0f,2.2f,5.8f,8.5f,3.4f+pulse*.2f);body|=ellipsoid((float)x,(float)y,(float)z, 0.0f, 1.0f,2.2f,8.0f,5.5f,3.8f);bool cleft=ellipsoid((float)x,(float)y,(float)z,0.0f,-5.0f,5.0f,2.2f,5.5f,4.5f);if(body&&!cleft){int v=(x*17+y*31+z*13)&7;COLORREF c=v<2?RGB(116,44,52):(v<5?RGB(145,54,62):RGB(171,67,73));out.push_back({x,y,z,c});}}}}
if(g_evo.hasPart(SPORE_PART_EYE)){for(int y=-5;y<=-2;++y)for(int x=3;x<=6;++x)for(int z=4;z<=7;++z)if(ellipsoid((float)x,(float)y,(float)z,4.5f,-3.3f,5.6f,2.3f,2.0f,2.0f))out.push_back({x,y,z,RGB(218,215,190)});out.push_back({5,-5,6,RGB(24,27,27)});}if(g_evo.hasPart(SPORE_PART_LEG)){for(int z=0;z<7;++z){out.push_back({-9,5,z,RGB(104,40,47)});out.push_back({9,5,z,RGB(104,40,47)});out.push_back({-5,9,z,RGB(104,40,47)});out.push_back({5,9,z,RGB(104,40,47)});}}if(g_evo.hasPart(SPORE_PART_MOUTH)){for(int x=-3;x<=3;++x)out.push_back({x,-9,2,RGB(54,20,25)});}}

static void drawCreature(HDC dc,int centerX,int centerY){POINT a=isoWorld(g_creature.getX(),g_creature.getY(),0,centerX,centerY);std::vector<MV> vox;buildVisceroid(vox);std::sort(vox.begin(),vox.end(),[](const MV&A,const MV&B){int da=A.y+A.x+A.z,db=B.y+B.x+B.z;return da<db;});const int scale=3;for(const MV&v:vox){int sx=a.x+(v.x-v.y)*scale;int sy=a.y+(v.x+v.y)*(scale/2)-8;drawMicroCube(dc,sx,sy,v.z,v.c,2);}}

static void drawCrystalCluster(HDC dc,int wx,int wy,COLORREF c,int centerX,int centerY){POINT a=isoWorld((float)wx+.5f,(float)wy+.5f,0,centerX,centerY);for(int stem=0;stem<3;++stem){int ox=(stem-1)*5,oy=(stem==1?-2:2),h=8+stem*3;for(int z=0;z<h;++z){int taper=z>h-3?(z-(h-3)):0;drawMicroCube(dc,a.x+ox,a.y+oy,z,shade(c,1.0f+.03f*stem),(std::max)(1,2-taper/2));}}}

static void addUpgrade(SporePartType type,const char*id,int bio,int cry,int vision,int speed,int damage,int armor){SporePartUpgrade u;u.type=type;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vision;u.speedDelta=speed;u.damageDelta=damage;u.armorDelta=armor;g_evo.unlock(u,g_biomass,g_crystals);}
static void initGame(){g_world.resize(80,56);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(36,24,6,180,1234u);g_world.addCrystalNode(47,31,7,260,777u);g_world.addCrystalNode(28,34,5,120,991u);g_creature.clear();g_creature.setPosition(40.0f,28.0f,0.0f);g_camX=40.0f;g_camY=28.0f;g_evo.resetToVisceroid();}
static void updateGame(float dt){float dx=0,dy=0;if(g_keys['A']||g_keys[VK_LEFT])dx-=1;if(g_keys['D']||g_keys[VK_RIGHT])dx+=1;if(g_keys['W']||g_keys[VK_UP])dy-=1;if(g_keys['S']||g_keys[VK_DOWN])dy+=1;if(dx&&dy){dx*=.7071067f;dy*=.7071067f;}float speed=4.4f+(float)g_evo.getSpeed()*.48f;if(!g_evo.canWalk())speed*=.60f;if(dx||dy)g_creature.move(dx,dy,dt*speed);float x=g_creature.getX(),y=g_creature.getY();x=(std::max)(3.0f,(std::min)(76.0f,x));y=(std::max)(3.0f,(std::min)(52.0f,y));g_creature.setPosition(x,y,0);float follow=1.0f-expf(-dt*5.2f);g_camX+=(x-g_camX)*follow;g_camY+=(y-g_camY)*follow;g_anim+=dt*5.5f;static unsigned int frame=0;g_world.update(frame++);}
static void text(HDC dc,int x,int y,const std::wstring&s){SetTextColor(dc,RGB(238,238,238));SetBkMode(dc,TRANSPARENT);TextOutW(dc,x,y,s.c_str(),(int)s.size());}

static void renderScene(HDC dc,RECT rc){fillSolid(dc,rc,RGB(24,29,27));int centerX=(rc.right-rc.left)/2,centerY=(rc.bottom-rc.top)*56/100;const int R=18;int minX=(int)floorf(g_camX)-R,maxX=(int)ceilf(g_camX)+R,minY=(int)floorf(g_camY)-R,maxY=(int)ceilf(g_camY)+R;for(int sum=minX+minY;sum<=maxX+maxY;++sum){for(int wx=minX;wx<=maxX;++wx){int wy=sum-wx;if(wy<minY||wy>maxY||!g_world.isInside(wx,wy))continue;VoxelMaterial m=g_world.getCell(wx,wy).material;COLORREF c=baseColor(m);if(m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN){drawGroundTile(dc,wx,wy,RGB(76,114,58),centerX,centerY);drawCrystalCluster(dc,wx,wy,c,centerX,centerY);}else{drawGroundTile(dc,wx,wy,c,centerX,centerY);}}}drawCreature(dc,centerX,centerY);RECT hud={16,16,730,116};fillSolid(dc,hud,RGB(12,14,14));text(dc,28,26,L"SPVersion - Visceroid microvoxel prototype");text(dc,28,49,L"WASD/flechas: mover   H: cosechar   1: ojo   2: piernas   3: boca");std::wstring s=L"Biomasa: "+std::to_wstring(g_biomass)+L"   Cristales: "+std::to_wstring(g_crystals)+L"   Vision: "+std::to_wstring(g_evo.getVision())+L"   Velocidad: "+std::to_wstring(g_evo.getSpeed());text(dc,28,72,s);text(dc,28,95,g_evo.canWalk()?L"Estado: locomocion evolucionada":L"Estado inicial: Visceroid rastrero");}

static void renderFrame(HWND h){RECT rc;GetClientRect(h,&rc);int w=rc.right-rc.left,ht=rc.bottom-rc.top;if(w<=0||ht<=0)return;ensureBackBuffer(h,w,ht);renderScene(g_backDC,rc);HDC wnd=GetDC(h);BitBlt(wnd,0,0,w,ht,g_backDC,0,0,SRCCOPY);ReleaseDC(h,wnd);}

static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l){switch(msg){case WM_ERASEBKGND:return 1;case WM_SIZE:destroyBackBuffer();return 0;case WM_DESTROY:destroyBackBuffer();PostQuitMessage(0);return 0;case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='1')addUpgrade(SPORE_PART_EYE,"eye_basic",25,10,4,0,0,0);if(w=='2')addUpgrade(SPORE_PART_LEG,"legs_basic",50,20,0,4,0,0);if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth_basic",35,15,0,0,3,0);if(w=='H'){int got=g_world.harvest((int)g_creature.getX(),(int)g_creature.getY(),8);g_crystals+=got;if(got>0)g_biomass+=got/2;}return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_KILLFOCUS:for(int i=0;i<256;++i)g_keys[i]=false;return 0;case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}}return DefWindowProcW(h,msg,w,l);}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPVoxelDemoWindow2";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"SPVersion - Visceroid Microvoxel Demo",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1360,860,NULL,NULL,inst,NULL);if(!g_hwnd)return 2;ShowWindow(g_hwnd,show);MSG msg;bool running=true;double accumulator=0.0;const double logicStep=1.0/120.0;double renderAccumulator=0.0;const double renderStep=1.0/60.0;while(running){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.05)dt=.05;accumulator+=dt;renderAccumulator+=dt;while(accumulator>=logicStep){updateGame((float)logicStep);accumulator-=logicStep;}if(renderAccumulator>=renderStep){renderFrame(g_hwnd);renderAccumulator=fmod(renderAccumulator,renderStep);}Sleep(1);}destroyBackBuffer();return 0;}
