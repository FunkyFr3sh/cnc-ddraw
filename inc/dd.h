#ifndef DD_H 
#define DD_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ddraw.h"

#ifdef __GNUC__
#define __try
#define __except(x) if (0)
#endif

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
HRESULT dd_TestCooperativeLevel();
HRESULT dd_GetDeviceIdentifier(LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags, REFIID riid);
HRESULT dd_CreateEx(GUID* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown* pUnkOuter);

#define FIX_CHILDS_DISABLED 0
#define FIX_CHILDS_DETECT 1
#define FIX_CHILDS_DETECT_PAINT 2
#define FIX_CHILDS_DETECT_HIDE 3

#define RESLIST_NORMAL 0
#define RESLIST_MINI 1
#define RESLIST_FULL 2

#define FILTER_NEAREST 0
#define FILTER_LINEAR 1
#define FILTER_CUBIC 2
#define FILTER_LANCZOS 3

#define SDM_MODE_SET_BY_GAME 0x00000001l
#define SDM_LEAVE_WINDOWED   0x00000002l
#define SDM_LEAVE_FULLSCREEN 0x00000004l

#define LIMIT_AUTO 0
#define LIMIT_TESTCOOP 1
#define LIMIT_BLTFAST 2

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#ifndef CREATE_WAITABLE_TIMER_MANUAL_RESET
#define CREATE_WAITABLE_TIMER_MANUAL_RESET 0x00000001
#endif

typedef struct SPEEDLIMITER
{
    DWORD tick_length;
    LONGLONG tick_length_ns;
    HANDLE htimer;
    LARGE_INTEGER due_time;
    BOOL dds_unlock_limiter_disabled;
} SPEEDLIMITER;

struct IDirectDrawSurfaceImpl;

extern struct CNCDDRAW g_ddraw;

typedef struct CNCDDRAW
{
    LONG ref;

    DWORD width;
    DWORD height;
    DWORD bpp;
    
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
        int width;
        int height;
        int opengl_y_align;

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
        float scale_x;
        float scale_y;
        float unscale_x;
        float unscale_y;

        int y_adjust;
        int x_adjust;
        RECT rc;
    } mouse;

    DWORD(WINAPI* renderer)(void);
    HWND hwnd;
    WNDPROC wndproc;
    struct { DWORD x; DWORD y; } cursor;
    int upscale_hack_width;
    int upscale_hack_height;
    BOOL isredalert;
    BOOL iscnc1;
    BOOL iskkndx;
    BOOL isworms2;
    LONG upscale_hack_active;
    HCURSOR old_cursor;
    int show_cursor_count;
    BOOL alt_key_down;
    BOOL bnet_active;
    BOOL bnet_was_fullscreen;
    BOOL bnet_was_upscaled;
    RECT bnet_win_rect;
    POINT bnet_pos;
    void* last_freed_palette; /* Dungeon Keeper hack */
    void* last_freed_surface; /* Nox hack */
    BOOL child_window_exists;
    HWND video_window_hwnd;
    BOOL got_child_windows;
    DWORD last_set_window_pos_tick; /* WINE hack */
    SPEEDLIMITER ticks_limiter;
    SPEEDLIMITER flip_limiter;
    DWORD minfps_tick_len;
    DWORD gui_thread_id;
    BOOL show_driver_warning;

    struct
    {
        HWND hwnd;
        int x;
        int y;
    } textbox; /* Age Of Empires 2 textbox align */


    struct
    {
        LONG frame_skip;
        BOOL enabled;
    } zoom;
} CNCDDRAW;

#endif
