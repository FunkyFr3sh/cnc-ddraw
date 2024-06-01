#ifndef WINAPI_HOOKS_H
#define WINAPI_HOOKS_H

#include <windows.h>


BOOL WINAPI fake_GetCursorPos(LPPOINT lpPoint);
BOOL WINAPI fake_ClipCursor(const RECT* lpRect);
int WINAPI fake_ShowCursor(BOOL bShow);
HCURSOR WINAPI fake_SetCursor(HCURSOR hCursor);
BOOL WINAPI fake_GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL WINAPI fake_GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL WINAPI fake_ClientToScreen(HWND hWnd, LPPOINT lpPoint);
BOOL WINAPI fake_ScreenToClient(HWND hWnd, LPPOINT lpPoint);
BOOL WINAPI fake_SetCursorPos(int X, int Y);
HWND WINAPI fake_WindowFromPoint(POINT Point);
BOOL WINAPI fake_GetClipCursor(LPRECT lpRect);
BOOL WINAPI fake_GetCursorInfo(PCURSORINFO pci);
int WINAPI fake_GetSystemMetrics(int nIndex);
BOOL WINAPI fake_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
BOOL WINAPI fake_MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
LRESULT WINAPI fake_SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LONG WINAPI fake_SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
LONG WINAPI fake_GetWindowLongA(HWND hWnd, int nIndex);
BOOL WINAPI fake_EnableWindow(HWND hWnd, BOOL bEnable);
BOOL WINAPI fake_DestroyWindow(HWND hWnd);
int WINAPI fake_MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints);
BOOL WINAPI fake_ShowWindow(HWND hWnd, int nCmdShow);
HWND WINAPI fake_GetTopWindow(HWND hWnd);
HWND WINAPI fake_GetForegroundWindow(void);
BOOL WINAPI fake_SetForegroundWindow(HWND hWnd);
HHOOK WINAPI fake_SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId);
BOOL WINAPI fake_PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
int WINAPI fake_GetDeviceCaps(HDC hdc, int index);

BOOL WINAPI fake_StretchBlt(
    HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop);

int WINAPI fake_SetDIBitsToDevice(
    HDC, int, int, DWORD, DWORD, int, int, UINT, UINT, const VOID*, const BITMAPINFO*, UINT);

int WINAPI fake_StretchDIBits(
    HDC, int, int, int, int, int, int, int, int, const VOID*, const BITMAPINFO*, UINT, DWORD);

HFONT WINAPI fake_CreateFontIndirectA(CONST LOGFONTA*);
HFONT WINAPI fake_CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCTSTR);

HMODULE WINAPI fake_LoadLibraryA(LPCSTR lpLibFileName);
HMODULE WINAPI fake_LoadLibraryW(LPCWSTR lpLibFileName);
HMODULE WINAPI fake_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE WINAPI fake_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
FARPROC WINAPI fake_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

BOOL WINAPI fake_GetDiskFreeSpaceA(
    LPCSTR lpRootPathName,
    LPDWORD lpSectorsPerCluster,
    LPDWORD lpBytesPerSector,
    LPDWORD lpNumberOfFreeClusters,
    LPDWORD lpTotalNumberOfClusters);

HWND WINAPI fake_CreateWindowExA(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
    int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

HRESULT WINAPI fake_CoCreateInstance(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);

MCIERROR WINAPI fake_mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam);

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI fake_SetUnhandledExceptionFilter(
    LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);

#endif
