#ifndef HOOK_H
#define HOOK_H

#include <windows.h>


#define HOOK_SKIP_2 0x00000001l
#define HOOK_LOCAL_ONLY 0x00000002l

typedef struct HOOKLISTDATA { 
    char function_name[32]; 
    PROC new_function; 
    PROC* function; 
    DWORD flags; 
    PROC org_function; 
    HMODULE mod;
} HOOKLISTDATA;

typedef struct HOOKLIST { char module_name[32]; HOOKLISTDATA data[34]; } HOOKLIST;

typedef BOOL(WINAPI* GETCURSORPOSPROC)(LPPOINT);
typedef BOOL(WINAPI* CLIPCURSORPROC)(const RECT*);
typedef int (WINAPI* SHOWCURSORPROC)(BOOL);
typedef HCURSOR(WINAPI* SETCURSORPROC)(HCURSOR);
typedef BOOL(WINAPI* GETWINDOWRECTPROC)(HWND, LPRECT);
typedef BOOL(WINAPI* GETCLIENTRECTPROC)(HWND, LPRECT);
typedef BOOL(WINAPI* CLIENTTOSCREENPROC)(HWND, LPPOINT);
typedef BOOL(WINAPI* SCREENTOCLIENTPROC)(HWND, LPPOINT);
typedef BOOL(WINAPI* SETCURSORPOSPROC)(int, int);
typedef HWND(WINAPI* WINDOWFROMPOINTPROC)(POINT);
typedef BOOL(WINAPI* GETCLIPCURSORPROC)(LPRECT);
typedef BOOL(WINAPI* GETCURSORINFOPROC)(PCURSORINFO);
typedef int (WINAPI* GETSYSTEMMETRICSPROC)(int);
typedef BOOL(WINAPI* SETWINDOWPOSPROC)(HWND, HWND, int, int, int, int, UINT);
typedef BOOL(WINAPI* MOVEWINDOWPROC)(HWND, int, int, int, int, BOOL);
typedef LRESULT(WINAPI* SENDMESSAGEAPROC)(HWND, UINT, WPARAM, LPARAM);
typedef LONG(WINAPI* SETWINDOWLONGAPROC)(HWND, int, LONG);
typedef LONG(WINAPI* GETWINDOWLONGAPROC)(HWND, int);
typedef BOOL(WINAPI* ENABLEWINDOWPROC)(HWND, BOOL);
typedef HWND(WINAPI* CREATEWINDOWEXAPROC)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef BOOL(WINAPI* DESTROYWINDOWPROC)(HWND);
typedef int (WINAPI* MAPWINDOWPOINTSPROC)(HWND, HWND, LPPOINT, UINT);
typedef BOOL (WINAPI* SHOWWINDOWPROC)(HWND, int);
typedef HWND(WINAPI* GETTOPWINDOWPROC)(HWND);
typedef HWND(WINAPI* GETFOREGROUNDWINDOWPROC)();
typedef BOOL(WINAPI* STRETCHBLTPROC)(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);
typedef BOOL(WINAPI* BITBLTPROC)(HDC, int, int, int, int, HDC, int, int, DWORD);

typedef int (WINAPI* SETDIBITSTODEVICEPROC)(
    HDC, int, int, DWORD, DWORD, int, int, UINT, UINT, const VOID*, const BITMAPINFO*, UINT);

typedef int (WINAPI* STRETCHDIBITSPROC)(
    HDC, int, int, int, int, int, int, int, int, const VOID*, const BITMAPINFO*, UINT, DWORD);

typedef BOOL (WINAPI* SETFOREGROUNDWINDOWPROC)(HWND);
typedef HHOOK(WINAPI* SETWINDOWSHOOKEXAPROC)(int, HOOKPROC, HINSTANCE, DWORD);
typedef BOOL(WINAPI* PEEKMESSAGEAPROC)(LPMSG, HWND, UINT, UINT, UINT);
typedef BOOL(WINAPI* GETMESSAGEAPROC)(LPMSG, HWND, UINT, UINT);
typedef BOOL(WINAPI* GETWINDOWPLACEMENTPROC)(HWND, WINDOWPLACEMENT*);
typedef BOOL(WINAPI* ENUMDISPLAYSETTINGSAPROC)(LPCSTR, DWORD, DEVMODEA*);
typedef SHORT(WINAPI* GETKEYSTATEPROC)(int);
typedef SHORT(WINAPI* GETASYNCKEYSTATEPROC)(int);

typedef int (WINAPI* GETDEVICECAPSPROC)(HDC, int);
typedef HFONT(WINAPI* CREATEFONTINDIRECTAPROC)(CONST LOGFONT*);
typedef HFONT(WINAPI* CREATEFONTAPROC)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCTSTR);
typedef UINT(WINAPI* GETSYSTEMPALETTEENTRIESPROC)(HDC, UINT, UINT, LPPALETTEENTRY);

typedef HMODULE(WINAPI* LOADLIBRARYAPROC)(LPCSTR);
typedef HMODULE(WINAPI* LOADLIBRARYWPROC)(LPCWSTR);
typedef HMODULE(WINAPI* LOADLIBRARYEXAPROC)(LPCSTR, HANDLE, DWORD);
typedef HMODULE(WINAPI* LOADLIBRARYEXWPROC)(LPCWSTR, HANDLE, DWORD);
typedef FARPROC(WINAPI* GETPROCADDRESSPROC)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* GETDISKFREESPACEAPROC)(LPCSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD);
typedef DWORD(WINAPI* GETVERSIONPROC)(void);
typedef BOOL(WINAPI* GETVERSIONEXAPROC)(LPOSVERSIONINFOA);
typedef HRESULT(WINAPI* COCREATEINSTANCEPROC)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
typedef MCIERROR(WINAPI* MCISENDCOMMANDAPROC)(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR);
typedef LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI* SETUNHANDLEDEXCEPTIONFILTERPROC)(LPTOP_LEVEL_EXCEPTION_FILTER);

extern GETCURSORPOSPROC real_GetCursorPos;
extern CLIPCURSORPROC real_ClipCursor;
extern SHOWCURSORPROC real_ShowCursor;
extern SETCURSORPROC real_SetCursor;
extern GETWINDOWRECTPROC real_GetWindowRect;
extern GETCLIENTRECTPROC real_GetClientRect;
extern CLIENTTOSCREENPROC real_ClientToScreen;
extern SCREENTOCLIENTPROC real_ScreenToClient;
extern SETCURSORPOSPROC real_SetCursorPos;
extern WINDOWFROMPOINTPROC real_WindowFromPoint;
extern GETCLIPCURSORPROC real_GetClipCursor;
extern GETCURSORINFOPROC real_GetCursorInfo;
extern GETSYSTEMMETRICSPROC real_GetSystemMetrics;
extern SETWINDOWPOSPROC real_SetWindowPos;
extern MOVEWINDOWPROC real_MoveWindow;
extern SENDMESSAGEAPROC real_SendMessageA;
extern SETWINDOWLONGAPROC real_SetWindowLongA;
extern GETWINDOWLONGAPROC real_GetWindowLongA;
extern ENABLEWINDOWPROC real_EnableWindow;
extern CREATEWINDOWEXAPROC real_CreateWindowExA;
extern DESTROYWINDOWPROC real_DestroyWindow;
extern MAPWINDOWPOINTSPROC real_MapWindowPoints;
extern SHOWWINDOWPROC real_ShowWindow;
extern GETTOPWINDOWPROC real_GetTopWindow;
extern GETFOREGROUNDWINDOWPROC real_GetForegroundWindow;
extern STRETCHBLTPROC real_StretchBlt;
extern BITBLTPROC real_BitBlt;
extern SETDIBITSTODEVICEPROC real_SetDIBitsToDevice;
extern STRETCHDIBITSPROC real_StretchDIBits;
extern SETFOREGROUNDWINDOWPROC real_SetForegroundWindow;
extern SETWINDOWSHOOKEXAPROC real_SetWindowsHookExA;
extern PEEKMESSAGEAPROC real_PeekMessageA;
extern GETMESSAGEAPROC real_GetMessageA;
extern GETWINDOWPLACEMENTPROC real_GetWindowPlacement;
extern ENUMDISPLAYSETTINGSAPROC real_EnumDisplaySettingsA;
extern GETKEYSTATEPROC real_GetKeyState;
extern GETASYNCKEYSTATEPROC real_GetAsyncKeyState;
extern GETDEVICECAPSPROC real_GetDeviceCaps;
extern CREATEFONTINDIRECTAPROC real_CreateFontIndirectA;
extern CREATEFONTAPROC real_CreateFontA;
extern GETSYSTEMPALETTEENTRIESPROC real_GetSystemPaletteEntries;
extern LOADLIBRARYAPROC real_LoadLibraryA;
extern LOADLIBRARYWPROC real_LoadLibraryW;
extern LOADLIBRARYEXAPROC real_LoadLibraryExA;
extern LOADLIBRARYEXWPROC real_LoadLibraryExW;
extern GETPROCADDRESSPROC real_GetProcAddress;
extern GETDISKFREESPACEAPROC real_GetDiskFreeSpaceA;
extern GETVERSIONPROC real_GetVersion;
extern GETVERSIONEXAPROC real_GetVersionExA;
extern COCREATEINSTANCEPROC real_CoCreateInstance;
extern MCISENDCOMMANDAPROC real_mciSendCommandA;
extern SETUNHANDLEDEXCEPTIONFILTERPROC real_SetUnhandledExceptionFilter;

extern BOOL g_hook_active;
extern HOOKLIST g_hook_hooklist[];

void hook_init();
void hook_exit();
void hook_patch_iat(HMODULE hmod, BOOL unhook, char* module_name, char* function_name, PROC new_function);
void hook_patch_iat_list(HMODULE hmod, BOOL unhook, HOOKLIST* hooks, BOOL is_local);
void hook_create(HOOKLIST* hooks, BOOL initial_hook);
void hook_revert(HOOKLIST* hooks);

#endif
