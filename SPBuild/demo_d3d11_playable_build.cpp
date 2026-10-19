#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#define SP_PLAYABLE_NO_ENTRYPOINT 1
#include "demo_d3d11_playable_mouse.cpp"
