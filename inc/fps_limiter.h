#ifndef FPS_LIMITER_H
#define FPS_LIMITER_H

#include <windows.h>
#include <dwmapi.h>


typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT {
    UINT hAdapter;
    UINT hDevice;
    UINT VidPnSourceId;
} D3DKMT_WAITFORVERTICALBLANKEVENT;

typedef struct _D3DKMT_OPENADAPTERFROMHDC {
    HDC  hDc;
    UINT hAdapter;
    LUID AdapterLuid;
    UINT VidPnSourceId;
} D3DKMT_OPENADAPTERFROMHDC;

typedef struct _D3DKMT_CLOSEADAPTER {
    UINT hAdapter;
} D3DKMT_CLOSEADAPTER;

typedef HRESULT(WINAPI* DWMFLUSHPROC)(VOID);
typedef HRESULT(WINAPI* DWMISCOMPOSITIONENABLEDPROC)(BOOL*);
typedef NTSTATUS(WINAPI* D3DKMTWAITFORVERTICALBLANKEVENTPROC)(const D3DKMT_WAITFORVERTICALBLANKEVENT* Arg1);
typedef NTSTATUS(WINAPI* D3DKMTOPENADAPTERFROMHDCPROC)(D3DKMT_OPENADAPTERFROMHDC* Arg1);
typedef NTSTATUS(WINAPI* D3DKMTCLOSEADAPTERPROC)(D3DKMT_CLOSEADAPTER* Arg1);

typedef struct FPSLIMITER
{
    DWORD tick_start;
    DWORD tick_end;
    DWORD tick_length;
    LONGLONG tick_length_ns;
    HANDLE htimer;
    LARGE_INTEGER due_time;
    D3DKMT_WAITFORVERTICALBLANKEVENT vblank_event;
    D3DKMT_OPENADAPTERFROMHDC adapter;
    D3DKMT_CLOSEADAPTER close_adapter;
    HMODULE gdi32_dll;
    HMODULE dwmapi_dll;
    DWMFLUSHPROC DwmFlush;
    DWMISCOMPOSITIONENABLEDPROC DwmIsCompositionEnabled;
    D3DKMTWAITFORVERTICALBLANKEVENTPROC D3DKMTWaitForVerticalBlankEvent;
    D3DKMTOPENADAPTERFROMHDCPROC D3DKMTOpenAdapterFromHdc;
    D3DKMTCLOSEADAPTERPROC D3DKMTCloseAdapter;
    BOOL got_adapter;
    BOOL initialized;
    BOOL is_wine;
} FPSLIMITER;

extern FPSLIMITER g_fpsl;

void fpsl_init();
BOOL fpsl_wait_for_vblank(BOOL open_adapter);
BOOL fpsl_dwm_flush();
BOOL fpsl_dwm_is_enabled();
void fpsl_frame_start();
void fpsl_frame_end();

#endif
