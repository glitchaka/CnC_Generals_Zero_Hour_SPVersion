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
static int g_biomass = 250;
static int g_crystals = 100;
static bool g_keys[256] = {};
static LARGE_INTEGER g_freq = {}, g_prev = {};

static COLORREF materialColor(VoxelMaterial m)
{
    switch (m)
    {
        case VOXEL_SOIL: return RGB(119, 100, 72);
        case VOXEL_GRASS: return RGB(92, 145, 78);
        case VOXEL_ROCK: return RGB(91, 94, 88);
        case VOXEL_PEACH_BIOMASS: return RGB(235, 168, 133);
        case VOXEL_CRYSTAL_CYAN: return RGB(69, 220, 232);
        case VOXEL_CRYSTAL_MAGENTA: return RGB(224, 79, 181);
        case VOXEL_CRYSTAL_GREEN: return RGB(117, 224, 114);
        case VOXEL_WATER: return RGB(66, 127, 185);
        default: return RGB(36, 42, 37);
    }
}

static void drawTextLine(HDC dc, int x, int y, const std::wstring &s)
{
    SetTextColor(dc, RGB(240,240,240));
    SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, x, y, s.c_str(), (int)s.size());
}

static void addUpgrade(SporePartType type, const char *id, int bio, int cry,
                       int vision, int speed, int damage, int armor)
{
    SporePartUpgrade u;
    u.type = type;
    u.id = id;
    u.biomassCost = bio;
    u.crystalCost = cry;
    u.visionDelta = vision;
    u.speedDelta = speed;
    u.damageDelta = damage;
    u.armorDelta = armor;
    g_evo.unlock(u, g_biomass, g_crystals);
}

static void initGame()
{
    g_world.resize(80, 56);
    g_world.generate(0xC0FFEEu);
    g_world.addCrystalNode(20, 16, 7, 180, 1234u);
    g_world.addCrystalNode(58, 35, 8, 260, 777u);
    g_world.addCrystalNode(42, 23, 5, 120, 991u);

    g_creature.clear();
    g_creature.setPosition(40.0f, 28.0f, 0.0f);
    g_evo.resetToVisceroid();
}

static void updateGame(float dt)
{
    float dx = 0.0f, dy = 0.0f;
    if (g_keys['A'] || g_keys[VK_LEFT])  dx -= 1.0f;
    if (g_keys['D'] || g_keys[VK_RIGHT]) dx += 1.0f;
    if (g_keys['W'] || g_keys[VK_UP])    dy -= 1.0f;
    if (g_keys['S'] || g_keys[VK_DOWN])  dy += 1.0f;

    float speed = 3.2f + (float)g_evo.getSpeed() * 0.35f;
    if (!g_evo.canWalk()) speed *= 0.48f;

    if (dx != 0.0f || dy != 0.0f)
        g_creature.move(dx, dy, dt * speed);

    float x = g_creature.getX();
    float y = g_creature.getY();
    if (x < 1.0f) x = 1.0f; if (x > 78.0f) x = 78.0f;
    if (y < 1.0f) y = 1.0f; if (y > 54.0f) y = 54.0f;
    g_creature.setPosition(x,y,0.0f);

    static unsigned int frame = 0;
    g_world.update(frame++);
}

static void render(HDC dc, RECT rc)
{
    FillRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    const int cell = 12;
    const int viewW = (rc.right-rc.left) / cell;
    const int viewH = (rc.bottom-rc.top) / cell;
    int cx = (int)g_creature.getX();
    int cy = (int)g_creature.getY();
    int startX = cx - viewW/2;
    int startY = cy - viewH/2;

    for (int sy = 0; sy < viewH; ++sy)
    {
        for (int sx = 0; sx < viewW; ++sx)
        {
            int wx = startX + sx;
            int wy = startY + sy;
            RECT r = { sx*cell, sy*cell, sx*cell+cell+1, sy*cell+cell+1 };
            COLORREF c = RGB(30,34,31);
            if (g_world.isInside(wx,wy)) c = materialColor(g_world.getCell(wx,wy).material);
            HBRUSH b = CreateSolidBrush(c);
            FillRect(dc,&r,b);
            DeleteObject(b);

            if (g_world.isInside(wx,wy))
            {
                VoxelMaterial m = g_world.getCell(wx,wy).material;
                if (m == VOXEL_CRYSTAL_CYAN || m == VOXEL_CRYSTAL_MAGENTA || m == VOXEL_CRYSTAL_GREEN)
                {
                    POINT p[4] = {
                        { sx*cell + cell/2, sy*cell + 1 },
                        { sx*cell + cell - 2, sy*cell + cell/2 },
                        { sx*cell + cell/2, sy*cell + cell - 1 },
                        { sx*cell + 2, sy*cell + cell/2 }
                    };
                    HBRUSH cb = CreateSolidBrush(RGB(245,245,255));
                    HGDIOBJ old = SelectObject(dc, cb);
                    Polygon(dc,p,4);
                    SelectObject(dc,old);
                    DeleteObject(cb);
                }
            }
        }
    }

    int px = (cx-startX)*cell + cell/2;
    int py = (cy-startY)*cell + cell/2;

    // Visceroid body.
    HBRUSH shadow = CreateSolidBrush(RGB(45,35,32));
    HGDIOBJ old = SelectObject(dc, shadow);
    Ellipse(dc, px-15, py+6, px+15, py+15);
    SelectObject(dc, old); DeleteObject(shadow);

    HBRUSH body = CreateSolidBrush(RGB(214,111,122));
    old = SelectObject(dc, body);
    Ellipse(dc, px-14, py-10, px+14, py+12);
    Ellipse(dc, px-9, py-15, px+7, py+5);
    SelectObject(dc, old); DeleteObject(body);

    if (g_evo.hasPart(SPORE_PART_EYE))
    {
        HBRUSH white = CreateSolidBrush(RGB(245,245,235)); old=SelectObject(dc,white);
        Ellipse(dc,px-5,py-8,px+2,py-1); SelectObject(dc,old); DeleteObject(white);
        HBRUSH black = CreateSolidBrush(RGB(25,25,25)); old=SelectObject(dc,black);
        Ellipse(dc,px-2,py-6,px+1,py-3); SelectObject(dc,old); DeleteObject(black);
    }
    if (g_evo.hasPart(SPORE_PART_LEG))
    {
        HPEN pen = CreatePen(PS_SOLID,4,RGB(167,79,95)); old=SelectObject(dc,pen);
        MoveToEx(dc,px-7,py+7,NULL); LineTo(dc,px-12,py+18);
        MoveToEx(dc,px+7,py+7,NULL); LineTo(dc,px+12,py+18);
        SelectObject(dc,old); DeleteObject(pen);
    }
    if (g_evo.hasPart(SPORE_PART_MOUTH))
    {
        HPEN pen = CreatePen(PS_SOLID,2,RGB(55,26,31)); old=SelectObject(dc,pen);
        MoveToEx(dc,px-5,py+3,NULL); LineTo(dc,px+5,py+3);
        SelectObject(dc,old); DeleteObject(pen);
    }

    RECT hud = { 12,12,520,118 };
    HBRUSH hb = CreateSolidBrush(RGB(20,20,22)); FillRect(dc,&hud,hb); DeleteObject(hb);
    drawTextLine(dc,24,24,L"SPVersion - demo visible del nucleo moderno");
    drawTextLine(dc,24,46,L"WASD/flechas: mover  |  H: cosechar  |  1: ojo  2: piernas  3: boca");
    std::wstring s = L"Biomasa: " + std::to_wstring(g_biomass) + L"   Cristales: " + std::to_wstring(g_crystals)
                   + L"   Vision: " + std::to_wstring(g_evo.getVision()) + L"   Velocidad: " + std::to_wstring(g_evo.getSpeed());
    drawTextLine(dc,24,68,s);
    drawTextLine(dc,24,90,g_evo.canWalk() ? L"Estado: criatura con locomocion" : L"Estado: visceroide rastrero");
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    switch(msg)
    {
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_KEYDOWN:
            if (w < 256) g_keys[w] = true;
            if (w == '1') addUpgrade(SPORE_PART_EYE,"eye_basic",25,10,4,0,0,0);
            if (w == '2') addUpgrade(SPORE_PART_LEG,"legs_basic",50,20,0,4,0,0);
            if (w == '3') addUpgrade(SPORE_PART_MOUTH,"mouth_basic",35,15,0,0,3,0);
            if (w == 'H') {
                int got = g_world.harvest((int)g_creature.getX(), (int)g_creature.getY(), 8);
                g_crystals += got;
                if (got > 0) g_biomass += got/2;
            }
            InvalidateRect(h,NULL,FALSE);
            return 0;
        case WM_KEYUP:
            if (w < 256) g_keys[w] = false;
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT rc; GetClientRect(h,&rc); render(dc,rc); EndPaint(h,&ps); return 0;
        }
    }
    return DefWindowProcW(h,msg,w,l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
    QueryPerformanceFrequency(&g_freq); QueryPerformanceCounter(&g_prev); initGame();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"SPVersionDemoWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!RegisterClassW(&wc)) return 1;

    HWND h = CreateWindowExW(0,wc.lpszClassName,L"SPVersion Creature/Voxel Demo",WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,CW_USEDEFAULT,1100,760,NULL,NULL,inst,NULL);
    if (!h) return 2;
    ShowWindow(h,show);

    MSG msg;
    bool running=true;
    while(running)
    {
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
        {
            if (msg.message==WM_QUIT) { running=false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        float dt=(float)(now.QuadPart-g_prev.QuadPart)/(float)g_freq.QuadPart;
        g_prev=now; if (dt>0.05f) dt=0.05f;
        updateGame(dt);
        InvalidateRect(h,NULL,FALSE);
        Sleep(8);
    }
    return 0;
}
