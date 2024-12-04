#ifndef DD_H 
#define DD_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ddraw.h"


typedef HRESULT(WINAPI* DIRECTDRAWCREATEPROC)(GUID FAR*, LPDIRECTDRAW FAR*, IUnknown FAR*);

ULONG dd_AddRef();
ULONG dd_Release();
HRESULT dd_EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback);
HRESULT dd_WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent);
HRESULT dd_SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwFlags);
HRESULT dd_SetCooperativeLevel(HWND hwnd, DWORD dwFlags);
HRESULT dd_RestoreDisplayMode();
HRESULT dd_GetCaps(LPDDCAPS_DX1 lpDDDriverCaps, LPDDCAPS_DX1 lpDDEmulCaps);
HRESULT dd_GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc);
HRESULT dd_GetMonitorFrequency(LPDWORD lpdwFreq);
HRESULT dd_GetAvailableVidMem(LPDDSCAPS lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree);
HRESULT dd_GetVerticalBlankStatus(LPBOOL lpbIsInVB);
HRESULT dd_GetDeviceIdentifier(LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags, REFIID riid);
HRESULT dd_CreateEx(GUID* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown* pUnkOuter);

#define FIX_CHILDS_DISABLED 0
#define FIX_CHILDS_DETECT 1
#define FIX_CHILDS_DETECT_PAINT 2
#define FIX_CHILDS_DETECT_HIDE 3

#define RESLIST_NORMAL 0
#define RESLIST_MINI 1
#define RESLIST_FULL 2

#define SDM_MODE_SET_BY_GAME 0x00000001l
#define SDM_LEAVE_WINDOWED   0x00000002l
#define SDM_LEAVE_FULLSCREEN 0x00000004l

typedef struct SPEEDLIMITER
{
    DWORD tick_length;
    LONGLONG tick_length_ns;
    HANDLE htimer;
    LARGE_INTEGER due_time;
    BOOL use_blt_or_flip;
} SPEEDLIMITER;

struct IDirectDrawSurfaceImpl;

extern struct CNCDDRAW* g_ddraw;

typedef struct CNCDDRAW
{
    ULONG ref;

    DWORD width;
    DWORD height;
    DWORD bpp;
    BOOL windowed;
    BOOL border;
    BOOL boxing;
    DEVMODE mode;
    struct IDirectDrawSurfaceImpl* primary;
    char title[128];
    CRITICAL_SECTION cs;

    /* real export from system32\ddraw.dll */
    HMODULE real_dll;
    DIRECTDRAWCREATEPROC DirectDrawCreate;
    LPDIRECTDRAW real_dd;

    struct
    {
        int maxfps;
        int minfps;
        DWORD minfps_tick_len;
        int width;
        int height;
        int bpp;

        HDC hdc;
        int* tex;

        HANDLE thread;
        BOOL run;
        HANDLE sem;
        DEVMODE mode;
        struct { int width; int height; int x; int y; } viewport;

        LONG palette_updated;
        LONG surface_updated;
        LONG clear_screen;

        float scale_w;
        float scale_h;
        float unscale_w;
        float unscale_h;
    } render;

    struct
    {
        int y_adjust;
        int x_adjust;
        RECT rc;
    } mouse;

    struct
    {
        int toggle_fullscreen;
        int toggle_maximize;
        int unlock_cursor1;
        int unlock_cursor2;
        int screenshot;
    } hotkeys;

    HWND hwnd;
    WNDPROC wndproc;
    struct { DWORD x; DWORD y; } cursor;
    BOOL locked;
    BOOL adjmouse;
    BOOL devmode;
    BOOL vsync;
    BOOL vhack;
    int upscale_hack_width;
    int upscale_hack_height;
    BOOL isredalert;
    BOOL iscnc1;
    BOOL iskkndx;
    LONG upscale_hack_active;
    DWORD(WINAPI* renderer)(void);
    BOOL fullscreen;
    BOOL maintas;
    BOOL noactivateapp;
    char shader[MAX_PATH];
    BOOL wine;
    HCURSOR old_cursor;
    int show_cursor_count;
    BOOL accurate_timers;
    BOOL resizable;
    BOOL toggle_borderless;
    BOOL toggle_upscaled;
    BOOL nonexclusive;
    BOOL fixpitch;
    int fixchilds;
    BOOL fixnotresponding;
    BOOL locktopleft;
    BOOL d3d9linear;
    BOOL gdilinear;
    int resolutions;
    BOOL armadahack;
    BOOL tshack;
    int maxgameticks;
    BOOL alt_key_down;
    BOOL releasealt;
    BOOL bnet_active;
    BOOL bnet_was_fullscreen;
    BOOL bnet_was_upscaled;
    RECT bnet_win_rect;
    DEVMODE bnet_mode;
    POINT bnet_pos;
    void* last_freed_palette; /* Dungeon Keeper hack */
    BOOL child_window_exists;
    BOOL got_child_windows;
    DWORD last_set_window_pos_tick; /* WINE hack */
    BOOL show_driver_warning;
    SPEEDLIMITER ticks_limiter;
    SPEEDLIMITER flip_limiter;

} CNCDDRAW;

#endif
