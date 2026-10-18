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
static int g_biomass=250, g_crystals=100;
static bool g_keys[256]={};
static LARGE_INTEGER g_freq={}, g_prev={};
static float g_anim=0.0f;

static COLORREF shade(COLORREF c,float k){int r=(int)(GetRValue(c)*k),g=(int)(GetGValue(c)*k),b=(int)(GetBValue(c)*k); if(r>255)r=255;if(g>255)g=255;if(b>255)b=255;return RGB(r,g,b);}
static COLORREF baseColor(VoxelMaterial m){switch(m){case VOXEL_SOIL:return RGB(111,88,61);case VOXEL_GRASS:return RGB(71,127,58);case VOXEL_ROCK:return RGB(100,103,98);case VOXEL_PEACH_BIOMASS:return RGB(235,155,118);case VOXEL_CRYSTAL_CYAN:return RGB(53,224,239);case VOXEL_CRYSTAL_MAGENTA:return RGB(226,71,189);case VOXEL_CRYSTAL_GREEN:return RGB(110,226,106);case VOXEL_WATER:return RGB(60,117,178);default:return RGB(38,44,40);}}
static int voxelHeight(VoxelMaterial m){switch(m){case VOXEL_ROCK:return 18;case VOXEL_PEACH_BIOMASS:return 12;case VOXEL_CRYSTAL_CYAN:case VOXEL_CRYSTAL_MAGENTA:case VOXEL_CRYSTAL_GREEN:return 36;case VOXEL_GRASS:return 6;case VOXEL_SOIL:return 5;default:return 3;}}

static void polyFill(HDC dc,POINT*p,int n,COLORREF c){HBRUSH b=CreateSolidBrush(c);HGDIOBJ ob=SelectObject(dc,b);HPEN pen=CreatePen(PS_SOLID,1,shade(c,.68f));HGDIOBJ op=SelectObject(dc,pen);Polygon(dc,p,n);SelectObject(dc,op);DeleteObject(pen);SelectObject(dc,ob);DeleteObject(b);}

static POINT isoRel(int wx,int wy,int z,int cx,int cy,int centerX,int centerY){const int TW=44,TH=22;int rx=wx-cx,ry=wy-cy;POINT p={centerX+(rx-ry)*(TW/2),centerY+(rx+ry)*(TH/2)-z};return p;}

static void drawVoxel(HDC dc,int wx,int wy,int z,int h,COLORREF c,int cx,int cy,int centerX,int centerY){POINT t=isoRel(wx,wy,z+h,cx,cy,centerX,centerY),l=isoRel(wx,wy+1,z+h,cx,cy,centerX,centerY),b=isoRel(wx+1,wy+1,z+h,cx,cy,centerX,centerY),r=isoRel(wx+1,wy,z+h,cx,cy,centerX,centerY);POINT l0=isoRel(wx,wy+1,z,cx,cy,centerX,centerY),b0=isoRel(wx+1,wy+1,z,cx,cy,centerX,centerY),r0=isoRel(wx+1,wy,z,cx,cy,centerX,centerY);POINT left[4]={l,b,b0,l0},right[4]={r,b,b0,r0},top[4]={t,r,b,l};polyFill(dc,left,4,shade(c,.72f));polyFill(dc,right,4,shade(c,.86f));polyFill(dc,top,4,shade(c,1.08f));}

static void drawCrystal(HDC dc,int wx,int wy,COLORREF c,int cx,int cy,int centerX,int centerY){drawVoxel(dc,wx,wy,6,30,c,cx,cy,centerX,centerY);POINT tip=isoRel(wx,wy,52,cx,cy,centerX,centerY),a=isoRel(wx,wy,36,cx,cy,centerX,centerY),b=isoRel(wx+1,wy,36,cx,cy,centerX,centerY),d=isoRel(wx,wy+1,36,cx,cy,centerX,centerY),e=isoRel(wx+1,wy+1,36,cx,cy,centerX,centerY);POINT f1[3]={tip,a,b},f2[3]={tip,b,e},f3[3]={tip,e,d},f4[3]={tip,d,a};polyFill(dc,f1,3,shade(c,1.30f));polyFill(dc,f2,3,shade(c,.98f));polyFill(dc,f3,3,shade(c,.76f));polyFill(dc,f4,3,shade(c,1.12f));}

static void drawCreature(HDC dc,int cx,int cy,int centerX,int centerY){COLORREF body=RGB(189,68,96);int bob=(int)(sinf(g_anim)*3.0f);int x=(int)g_creature.getX(),y=(int)g_creature.getY();
    // wide amorphous visceroid body: clearly visible from spawn
    drawVoxel(dc,x-1,y,10+bob,18,shade(body,.90f),cx,cy,centerX,centerY);
    drawVoxel(dc,x,y,11+bob,28,body,cx,cy,centerX,centerY);
    drawVoxel(dc,x+1,y,10+bob,24,shade(body,1.05f),cx,cy,centerX,centerY);
    drawVoxel(dc,x,y+1,9+bob,20,shade(body,.96f),cx,cy,centerX,centerY);
    drawVoxel(dc,x+1,y+1,8+bob,15,shade(body,.86f),cx,cy,centerX,centerY);
    drawVoxel(dc,x-1,y+1,8+bob,14,shade(body,.84f),cx,cy,centerX,centerY);
    if(g_evo.hasPart(SPORE_PART_EYE)){drawVoxel(dc,x+1,y-1,25+bob,10,RGB(238,235,210),cx,cy,centerX,centerY);drawVoxel(dc,x+1,y-1,35+bob,4,RGB(22,23,26),cx,cy,centerX,centerY);}
    if(g_evo.hasPart(SPORE_PART_LEG)){drawVoxel(dc,x-1,y+2,0,22,RGB(143,55,79),cx,cy,centerX,centerY);drawVoxel(dc,x+2,y,0,22,RGB(143,55,79),cx,cy,centerX,centerY);}
    if(g_evo.hasPart(SPORE_PART_MOUTH)){drawVoxel(dc,x+1,y-1,12+bob,7,RGB(76,22,39),cx,cy,centerX,centerY);}
}

static void addUpgrade(SporePartType type,const char*id,int bio,int cry,int vision,int speed,int damage,int armor){SporePartUpgrade u;u.type=type;u.id=id;u.biomassCost=bio;u.crystalCost=cry;u.visionDelta=vision;u.speedDelta=speed;u.damageDelta=damage;u.armorDelta=armor;g_evo.unlock(u,g_biomass,g_crystals);}
static void initGame(){g_world.resize(80,56);g_world.generate(0xC0FFEEu);g_world.addCrystalNode(36,24,6,180,1234u);g_world.addCrystalNode(47,31,7,260,777u);g_world.addCrystalNode(28,34,5,120,991u);g_creature.clear();g_creature.setPosition(40.0f,28.0f,0.0f);g_evo.resetToVisceroid();}
static void updateGame(float dt){float dx=0,dy=0;if(g_keys['A']||g_keys[VK_LEFT])dx-=1;if(g_keys['D']||g_keys[VK_RIGHT])dx+=1;if(g_keys['W']||g_keys[VK_UP])dy-=1;if(g_keys['S']||g_keys[VK_DOWN])dy+=1;float speed=3.0f+(float)g_evo.getSpeed()*.40f;if(!g_evo.canWalk())speed*=.46f;if(dx||dy)g_creature.move(dx,dy,dt*speed);float x=g_creature.getX(),y=g_creature.getY();if(x<4)x=4;if(x>75)x=75;if(y<4)y=4;if(y>51)y=51;g_creature.setPosition(x,y,0);g_anim+=dt*7.0f;static unsigned int frame=0;g_world.update(frame++);}
static void text(HDC dc,int x,int y,const std::wstring&s){SetTextColor(dc,RGB(245,245,245));SetBkMode(dc,TRANSPARENT);TextOutW(dc,x,y,s.c_str(),(int)s.size());}

static void renderScene(HDC dc,RECT rc){HBRUSH bg=CreateSolidBrush(RGB(24,31,28));FillRect(dc,&rc,bg);DeleteObject(bg);int cx=(int)g_creature.getX(),cy=(int)g_creature.getY();int centerX=(rc.right-rc.left)/2,centerY=(rc.bottom-rc.top)*58/100;int R=20;
    for(int sum=-2*R;sum<=2*R;++sum){for(int rx=-R;rx<=R;++rx){int ry=sum-rx;if(ry<-R||ry>R)continue;int wx=cx+rx,wy=cy+ry;if(!g_world.isInside(wx,wy))continue;VoxelMaterial m=g_world.getCell(wx,wy).material;COLORREF c=baseColor(m);if(m==VOXEL_CRYSTAL_CYAN||m==VOXEL_CRYSTAL_MAGENTA||m==VOXEL_CRYSTAL_GREEN)drawCrystal(dc,wx,wy,c,cx,cy,centerX,centerY);else drawVoxel(dc,wx,wy,0,voxelHeight(m),c,cx,cy,centerX,centerY);}}
    drawCreature(dc,cx,cy,centerX,centerY);
    RECT hud={18,18,760,122};HBRUSH hb=CreateSolidBrush(RGB(15,16,18));FillRect(dc,&hud,hb);DeleteObject(hb);text(dc,30,28,L"SPVersion - visceroide voxel / prototipo de evolucion");text(dc,30,51,L"WASD/flechas: mover   H: cosechar   1: ojo   2: piernas   3: boca");std::wstring s=L"Biomasa: "+std::to_wstring(g_biomass)+L"   Cristales: "+std::to_wstring(g_crystals)+L"   Vision: "+std::to_wstring(g_evo.getVision())+L"   Velocidad: "+std::to_wstring(g_evo.getSpeed());text(dc,30,74,s);text(dc,30,97,g_evo.canWalk()?L"Estado: criatura con locomocion":L"Estado inicial: visceroide rastrero");}

static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l){switch(msg){case WM_ERASEBKGND:return 1;case WM_DESTROY:PostQuitMessage(0);return 0;case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='1')addUpgrade(SPORE_PART_EYE,"eye_basic",25,10,4,0,0,0);if(w=='2')addUpgrade(SPORE_PART_LEG,"legs_basic",50,20,0,4,0,0);if(w=='3')addUpgrade(SPORE_PART_MOUTH,"mouth_basic",35,15,0,0,3,0);if(w=='H'){int got=g_world.harvest((int)g_creature.getX(),(int)g_creature.getY(),8);g_crystals+=got;if(got>0)g_biomass+=got/2;}return 0;case WM_KEYUP:if(w<256)g_keys[w]=false;return 0;case WM_PAINT:{PAINTSTRUCT ps;HDC wnd=BeginPaint(h,&ps);RECT rc;GetClientRect(h,&rc);HDC mem=CreateCompatibleDC(wnd);HBITMAP bmp=CreateCompatibleBitmap(wnd,rc.right,rc.bottom);HGDIOBJ old=SelectObject(mem,bmp);renderScene(mem,rc);BitBlt(wnd,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}}return DefWindowProcW(h,msg,w,l);}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){QueryPerformanceFrequency(&g_freq);QueryPerformanceCounter(&g_prev);initGame();WNDCLASSW wc={};wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.lpszClassName=L"SPVoxelDemoWindow";wc.hCursor=LoadCursor(NULL,IDC_ARROW);if(!RegisterClassW(&wc))return 1;HWND h=CreateWindowExW(0,wc.lpszClassName,L"SPVersion - Visceroid Voxel Evolution Demo",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,820,NULL,NULL,inst,NULL);if(!h)return 2;ShowWindow(h,show);MSG msg;bool running=true;double accumulator=0.0;const double step=1.0/120.0;while(running){while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}LARGE_INTEGER now;QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-g_prev.QuadPart)/(double)g_freq.QuadPart;g_prev=now;if(dt>.05)dt=.05;accumulator+=dt;while(accumulator>=step){updateGame((float)step);accumulator-=step;}InvalidateRect(h,NULL,FALSE);UpdateWindow(h);Sleep(1);}return 0;}
