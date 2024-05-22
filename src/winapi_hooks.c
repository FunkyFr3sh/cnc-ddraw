#include <windows.h>
#include <windowsx.h>
#include <math.h>
#include "debug.h"
#include "config.h"
#include "dd.h"
#include "ddraw.h"
#include "hook.h"
#include "config.h"
#include "utils.h"
#include "mouse.h"
#include "wndproc.h"
#include "render_gdi.h"


BOOL WINAPI fake_GetCursorPos(LPPOINT lpPoint)
{
    POINT pt, realpt;

    if (!real_GetCursorPos(&pt) || !g_ddraw)
        return FALSE;

    realpt.x = pt.x;
    realpt.y = pt.y;

    if (g_ddraw->locked && (!g_ddraw->windowed || real_ScreenToClient(g_ddraw->hwnd, &pt)))
    {
        /* fallback solution for possible ClipCursor failure */
        int diffx = 0, diffy = 0;

        int max_width = g_ddraw->adjmouse ? g_ddraw->render.viewport.width : g_ddraw->width;
        int max_height = g_ddraw->adjmouse ? g_ddraw->render.viewport.height : g_ddraw->height;

        pt.x -= g_ddraw->mouse.x_adjust;
        pt.y -= g_ddraw->mouse.y_adjust;

        if (pt.x < 0)
        {
            diffx = pt.x;
            pt.x = 0;
        }

        if (pt.y < 0)
        {
            diffy = pt.y;
            pt.y = 0;
        }

        if (pt.x > max_width)
        {
            diffx = pt.x - max_width;
            pt.x = max_width;
        }

        if (pt.y > max_height)
        {
            diffy = pt.y - max_height;
            pt.y = max_height;
        }

        if (diffx || diffy)
            real_SetCursorPos(realpt.x - diffx, realpt.y - diffy);

        int x = 0;
        int y = 0;

        if (g_ddraw->adjmouse)
        {
            x = min((DWORD)(roundf(pt.x * g_ddraw->render.unscale_w)), g_ddraw->width);
            y = min((DWORD)(roundf(pt.y * g_ddraw->render.unscale_h)), g_ddraw->height);
        }
        else
        {
            x = pt.x;
            y = pt.y;
        }

        if (g_ddraw->vhack && InterlockedExchangeAdd(&g_ddraw->upscale_hack_active, 0))
        {
            diffx = 0;
            diffy = 0;

            if (x > g_ddraw->upscale_hack_width)
            {
                diffx = x - g_ddraw->upscale_hack_width;
                x = g_ddraw->upscale_hack_width;
            }

            if (y > g_ddraw->upscale_hack_height)
            {
                diffy = y - g_ddraw->upscale_hack_height;
                y = g_ddraw->upscale_hack_height;
            }

            if (diffx || diffy)
                real_SetCursorPos(realpt.x - diffx, realpt.y - diffy);
        }

        InterlockedExchange((LONG*)&g_ddraw->cursor.x, x);
        InterlockedExchange((LONG*)&g_ddraw->cursor.y, y);

        if (lpPoint)
        {
            lpPoint->x = x;
            lpPoint->y = y;
        }

        return TRUE;
    }
    else if (lpPoint && g_ddraw->bnet_active && real_ScreenToClient(g_ddraw->hwnd, &pt))
    {
        if (pt.x > 0 && pt.x < g_ddraw->width && pt.y > 0 && pt.y < g_ddraw->height)
        {
            lpPoint->x = pt.x;
            lpPoint->y = pt.y;

            return TRUE;
        }
    }

    if (lpPoint)
    {
        lpPoint->x = InterlockedExchangeAdd((LONG*)&g_ddraw->cursor.x, 0);
        lpPoint->y = InterlockedExchangeAdd((LONG*)&g_ddraw->cursor.y, 0);
    }

    return TRUE;
}

BOOL WINAPI fake_ClipCursor(const RECT* lpRect)
{
    if (g_ddraw)
    {
        RECT dst_rc = {
            0,
            0,
            g_ddraw->width,
            g_ddraw->height
        };

        if (lpRect)
            CopyRect(&dst_rc, lpRect);

        if (g_ddraw->adjmouse)
        {
            dst_rc.left = (LONG)(roundf(dst_rc.left * g_ddraw->render.scale_w));
            dst_rc.top = (LONG)(roundf(dst_rc.top * g_ddraw->render.scale_h));
            dst_rc.bottom = (LONG)(roundf(dst_rc.bottom * g_ddraw->render.scale_h));
            dst_rc.right = (LONG)(roundf(dst_rc.right * g_ddraw->render.scale_w));
        }

        int max_width = g_ddraw->adjmouse ? g_ddraw->render.viewport.width : g_ddraw->width;
        int max_height = g_ddraw->adjmouse ? g_ddraw->render.viewport.height : g_ddraw->height;

        dst_rc.bottom = min(dst_rc.bottom, max_height);
        dst_rc.right = min(dst_rc.right, max_width);

        OffsetRect(
            &dst_rc,
            g_ddraw->mouse.x_adjust, 
            g_ddraw->mouse.y_adjust);

        CopyRect(&g_ddraw->mouse.rc, &dst_rc);

        if (g_ddraw->locked)
        {
            real_MapWindowPoints(g_ddraw->hwnd, HWND_DESKTOP, (LPPOINT)&dst_rc, 2);

            return real_ClipCursor(&dst_rc);
        }
    }

    return TRUE;
}

int WINAPI fake_ShowCursor(BOOL bShow)
{
    if (g_ddraw)
    {
        if (g_ddraw->locked || g_ddraw->devmode)
        {
            int count = real_ShowCursor(bShow);
            InterlockedExchange((LONG*)&g_ddraw->show_cursor_count, count);
            return count;
        }
        else
        {
            return bShow ?
                InterlockedIncrement((LONG*)&g_ddraw->show_cursor_count) :
                InterlockedDecrement((LONG*)&g_ddraw->show_cursor_count);
        }
    }

    return real_ShowCursor(bShow);
}

HCURSOR WINAPI fake_SetCursor(HCURSOR hCursor)
{
    if (g_ddraw)
    {
        HCURSOR cursor = (HCURSOR)InterlockedExchange((LONG*)&g_ddraw->old_cursor, (LONG)hCursor);

        if (!g_ddraw->locked && !g_ddraw->devmode)
            return cursor;
    }

    return real_SetCursor(hCursor);
}

BOOL WINAPI fake_GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    if (lpRect &&
        g_ddraw &&
        g_ddraw->hwnd &&
        (g_hook_method != 2 || g_ddraw->renderer == gdi_render_main))
    {
        if (g_ddraw->hwnd == hWnd)
        {
            lpRect->bottom = g_ddraw->height;
            lpRect->left = 0;
            lpRect->right = g_ddraw->width;
            lpRect->top = 0;

            return TRUE;
        }
        else
        {
            if (real_GetWindowRect(hWnd, lpRect))
            {
                real_MapWindowPoints(HWND_DESKTOP, g_ddraw->hwnd, (LPPOINT)lpRect, 2);

                return TRUE;
            }

            return FALSE;
        }
    }

    return real_GetWindowRect(hWnd, lpRect);
}

BOOL WINAPI fake_GetClientRect(HWND hWnd, LPRECT lpRect)
{
    if (lpRect &&
        g_ddraw &&
        g_ddraw->hwnd == hWnd &&
        (g_hook_method != 2 || g_ddraw->renderer == gdi_render_main))
    {
        lpRect->bottom = g_ddraw->height;
        lpRect->left = 0;
        lpRect->right = g_ddraw->width;
        lpRect->top = 0;

        return TRUE;
    }

    return real_GetClientRect(hWnd, lpRect);
}

BOOL WINAPI fake_ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    if (g_ddraw && g_ddraw->hwnd != hWnd)
        return real_ClientToScreen(hWnd, lpPoint) && real_ScreenToClient(g_ddraw->hwnd, lpPoint);

    return TRUE;
}

BOOL WINAPI fake_ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    if (g_ddraw && g_ddraw->hwnd != hWnd)
        return real_ClientToScreen(g_ddraw->hwnd, lpPoint) && real_ScreenToClient(hWnd, lpPoint);

    return TRUE;
}

BOOL WINAPI fake_SetCursorPos(int X, int Y)
{
    if (g_ddraw && !g_ddraw->locked && !g_ddraw->devmode)
        return TRUE;

    POINT pt = { X, Y };

    if (g_ddraw)
    {
        if (g_ddraw->adjmouse)
        {
            pt.x = (LONG)(roundf(pt.x * g_ddraw->render.scale_w));
            pt.y = (LONG)(roundf(pt.y * g_ddraw->render.scale_h));
        }

        pt.x += g_ddraw->mouse.x_adjust;
        pt.y += g_ddraw->mouse.y_adjust;
    }

    return g_ddraw && real_ClientToScreen(g_ddraw->hwnd, &pt) && real_SetCursorPos(pt.x, pt.y);
}

HWND WINAPI fake_WindowFromPoint(POINT Point)
{
    POINT pt = { Point.x, Point.y };
    return g_ddraw && real_ClientToScreen(g_ddraw->hwnd, &pt) ? real_WindowFromPoint(pt) : NULL;
}

BOOL WINAPI fake_GetClipCursor(LPRECT lpRect)
{
    if (lpRect && g_ddraw)
    {
        lpRect->bottom = g_ddraw->height;
        lpRect->left = 0;
        lpRect->right = g_ddraw->width;
        lpRect->top = 0;

        return TRUE;
    }

    return FALSE;
}

BOOL WINAPI fake_GetCursorInfo(PCURSORINFO pci)
{
    return pci && g_ddraw && real_GetCursorInfo(pci) && real_ScreenToClient(g_ddraw->hwnd, &pci->ptScreenPos);
}

int WINAPI fake_GetSystemMetrics(int nIndex)
{
    if (g_ddraw)
    {
        if (nIndex == SM_CXSCREEN)
            return g_ddraw->width;

        if (nIndex == SM_CYSCREEN)
            return g_ddraw->height;
    }

    return real_GetSystemMetrics(nIndex);
}

BOOL WINAPI fake_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    if (g_ddraw && g_ddraw->hwnd)
    {
        if (g_ddraw->hwnd == hWnd)
        {
            UINT req_flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER;

            if ((uFlags & req_flags) != req_flags)
                return TRUE;
        }
        else if (!IsChild(g_ddraw->hwnd, hWnd) && !(real_GetWindowLongA(hWnd, GWL_STYLE) & WS_CHILD))
        {
            POINT pt = { 0, 0 };
            if (real_ClientToScreen(g_ddraw->hwnd, &pt))
            {
                X += pt.x;
                Y += pt.y;
            }
        }
    }

    return real_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

BOOL WINAPI fake_MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    if (g_ddraw && g_ddraw->hwnd)
    {
        if (g_ddraw->hwnd == hWnd)
        {
            return TRUE;
        }
        else if (!IsChild(g_ddraw->hwnd, hWnd) && !(real_GetWindowLongA(hWnd, GWL_STYLE) & WS_CHILD))
        {
            POINT pt = { 0, 0 };
            if (real_ClientToScreen(g_ddraw->hwnd, &pt))
            {
                X += pt.x;
                Y += pt.y;
            }
        }
    }

    return real_MoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
}

LRESULT WINAPI fake_SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd && Msg == WM_MOUSEMOVE)
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (g_ddraw->adjmouse)
        {
            x = (int)(roundf(x * g_ddraw->render.scale_w));
            y = (int)(roundf(y * g_ddraw->render.scale_h));
        }

        lParam = MAKELPARAM(x + g_ddraw->mouse.x_adjust, y + g_ddraw->mouse.y_adjust);
    }

    if (g_ddraw && g_ddraw->hwnd == hWnd && Msg == WM_SIZE && (g_hook_method != 2 && g_hook_method != 3))
    {
        Msg = WM_SIZE_DDRAW;
    }

    LRESULT result = real_SendMessageA(hWnd, Msg, wParam, lParam);

    if (result && g_ddraw && Msg == CB_GETDROPPEDCONTROLRECT)
    {
        RECT* rc = (RECT*)lParam;
        if (rc)
            real_MapWindowPoints(HWND_DESKTOP, g_ddraw->hwnd, (LPPOINT)rc, 2);
    }

    return result;
}

static WNDPROC g_compat_wndproc;
LRESULT CALLBACK compat_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProcA(g_compat_wndproc, hWnd, uMsg, wParam, lParam);
}

LONG WINAPI fake_SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd)
    {
        if (nIndex == GWL_STYLE)
            return 0;

        if (nIndex == GWL_WNDPROC && g_ddraw->fixwndprochook)
        {
            if (dwNewLong == (LONG)compat_WndProc)
            {
                WNDPROC old = g_ddraw->wndproc = g_compat_wndproc;
                //g_compat_wndproc = NULL;
                return (LONG)old;
            }
            else
            {
                if (dwNewLong != (LONG)g_ddraw->wndproc)
                {
                    g_compat_wndproc = g_ddraw->wndproc;
                    g_ddraw->wndproc = (WNDPROC)dwNewLong;
                }

                return (LONG)compat_WndProc;
            }
        }
    }

    return real_SetWindowLongA(hWnd, nIndex, dwNewLong);
}

LONG WINAPI fake_GetWindowLongA(HWND hWnd, int nIndex)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd)
    {
        if (nIndex == GWL_WNDPROC && g_ddraw->fixwndprochook)
        {
            return (LONG)compat_WndProc;
        }
    }

    return real_GetWindowLongA(hWnd, nIndex);
}

BOOL WINAPI fake_EnableWindow(HWND hWnd, BOOL bEnable)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd)
    {
        return FALSE;
    }

    return real_EnableWindow(hWnd, bEnable);
}

int WINAPI fake_MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints)
{
    if (g_ddraw)
    {
        if (hWndTo == HWND_DESKTOP)
        {
            if (hWndFrom == g_ddraw->hwnd)
            {
                return 0;
            }
            else
            {
                //real_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);
                //return real_MapWindowPoints(HWND_DESKTOP, g_ddraw->hwnd, lpPoints, cPoints);
            }
        }

        if (hWndFrom == HWND_DESKTOP)
        {
            if (hWndTo == g_ddraw->hwnd)
            {
                return 0;
            }
            else
            {
                //real_MapWindowPoints(g_ddraw->hwnd, HWND_DESKTOP, lpPoints, cPoints);
                //return real_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);
            }
        }
    }

    return real_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);
}

BOOL WINAPI fake_ShowWindow(HWND hWnd, int nCmdShow)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd)
    {
        if (nCmdShow == SW_SHOWMAXIMIZED)
            nCmdShow = SW_SHOWNORMAL;

        if (nCmdShow == SW_MAXIMIZE)
            nCmdShow = SW_NORMAL;

        if (nCmdShow == SW_MINIMIZE && (g_hook_method != 2 && g_hook_method != 3))
            return TRUE;
    }

    return real_ShowWindow(hWnd, nCmdShow);
}

BOOL WINAPI fake_SetForegroundWindow(HWND hWnd)
{
    return TRUE;
}

HHOOK WINAPI fake_SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId)
{
    if (idHook == WH_KEYBOARD_LL && hmod && GetModuleHandle("AcGenral") == hmod)
    {
        return NULL;
    }

    if (idHook == WH_MOUSE && lpfn && !hmod && !g_mouse_hook && cfg_get_bool("fixmousehook", FALSE))
    {
        g_mouse_proc = lpfn;
        return g_mouse_hook = real_SetWindowsHookExA(idHook, mouse_hook_proc, hmod, dwThreadId);
    }

    return real_SetWindowsHookExA(idHook, lpfn, hmod, dwThreadId);
}

int WINAPI fake_GetDeviceCaps(HDC hdc, int index)
{
    if (g_ddraw &&
        g_ddraw->bpp &&
        index == BITSPIXEL &&
        (g_hook_method != 2 || g_ddraw->renderer == gdi_render_main))
    {
        return g_ddraw->bpp;
    }

    return real_GetDeviceCaps(hdc, index);
}

HFONT WINAPI fake_CreateFontIndirectA(CONST LOGFONTA* lplf)
{
    LOGFONTA lf;
    memcpy(&lf, lplf, sizeof(lf));
    lf.lfQuality = NONANTIALIASED_QUALITY;

    return real_CreateFontIndirectA(&lf);
}

HFONT WINAPI fake_CreateFontA(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight,
    DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet,
    DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily,
    LPCTSTR lpszFace)
{
    fdwQuality = NONANTIALIASED_QUALITY;

    return 
        real_CreateFontA(
            nHeight, nWidth, nEscapement, nOrientation, fnWeight,
            fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet,
            fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily,
            lpszFace);
}

HMODULE WINAPI fake_LoadLibraryA(LPCSTR lpLibFileName)
{
    HMODULE hmod = real_LoadLibraryA(lpLibFileName);

    hook_init();

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryW(LPCWSTR lpLibFileName)
{
    HMODULE hmod = real_LoadLibraryW(lpLibFileName);

    hook_init();

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod = real_LoadLibraryExA(lpLibFileName, hFile, dwFlags);

    hook_init();

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod = real_LoadLibraryExW(lpLibFileName, hFile, dwFlags);

    hook_init();

    return hmod;
}

BOOL WINAPI fake_DestroyWindow(HWND hWnd)
{
    BOOL result = real_DestroyWindow(hWnd);

    if (g_ddraw && g_ddraw->hwnd != hWnd && g_ddraw->bnet_active)
    {
        RedrawWindow(NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

        if (!FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL))
        {
            KillTimer(g_ddraw->hwnd, IDT_TIMER_REDRAW_BNET);
            g_ddraw->bnet_active = FALSE;
            SetFocus(g_ddraw->hwnd);
            mouse_lock();

            if (g_ddraw->windowed)
            {
                g_ddraw->bnet_pos.x = g_ddraw->bnet_pos.y = 0;
                real_ClientToScreen(g_ddraw->hwnd, &g_ddraw->bnet_pos);

                if (!g_ddraw->bnet_was_upscaled)
                {
                    int width = g_ddraw->bnet_win_rect.right - g_ddraw->bnet_win_rect.left;
                    int height = g_ddraw->bnet_win_rect.bottom - g_ddraw->bnet_win_rect.top;

                    UINT flags = width != g_ddraw->width || height != g_ddraw->height ? 0 : SWP_NOMOVE;

                    int dst_width = width == g_ddraw->width ? 0 : width;
                    int dst_height = height == g_ddraw->height ? 0 : height;

                    util_set_window_rect(
                        g_ddraw->bnet_win_rect.left, 
                        g_ddraw->bnet_win_rect.top, 
                        dst_width, 
                        dst_height, 
                        flags);
                }
            }

            g_ddraw->fullscreen = g_ddraw->bnet_was_upscaled;

            SetTimer(g_ddraw->hwnd, IDT_TIMER_LEAVE_BNET, 1000, (TIMERPROC)NULL);

            g_ddraw->resizable = TRUE;
        }
    }

    return result;
}

HWND WINAPI fake_CreateWindowExA(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
    int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    /* Fix for SMACKW32.DLL creating another window that steals the focus */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "MouseTypeWind") == 0 && g_ddraw)
    {
        dwStyle &= ~WS_VISIBLE;
    }

    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "SDlgDialog") == 0 && g_ddraw)
    {
        if (!g_ddraw->bnet_active)
        {
            g_ddraw->bnet_was_upscaled = g_ddraw->fullscreen;
            g_ddraw->fullscreen = FALSE;

            if (!g_ddraw->windowed && !g_ddraw->bnet_was_fullscreen)
            {
                if ((g_ddraw->render.mode.dmPelsWidth != 640 ||
                    g_ddraw->render.mode.dmPelsHeight != 480) &&
                    GetSystemMetrics(SM_CMONITORS) == 1)
                {
                    memcpy(&g_ddraw->bnet_mode, &g_ddraw->render.mode, sizeof(DEVMODE));

                    g_ddraw->render.mode.dmPelsWidth = 640;
                    g_ddraw->render.mode.dmPelsHeight = 480;
                    ChangeDisplaySettings(&g_ddraw->render.mode, CDS_FULLSCREEN);

                    g_ddraw->bnet_was_fullscreen = TRUE;
                }
                else
                {
                    real_SetWindowPos(
                        g_ddraw->hwnd,
                        0,
                        0,
                        0,
                        g_ddraw->render.width,
                        g_ddraw->render.height + 1,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
                }
            }

            real_GetClientRect(g_ddraw->hwnd, &g_ddraw->bnet_win_rect);
            real_MapWindowPoints(g_ddraw->hwnd, HWND_DESKTOP, (LPPOINT)&g_ddraw->bnet_win_rect, 2);

            int width = g_ddraw->bnet_win_rect.right - g_ddraw->bnet_win_rect.left;
            int height = g_ddraw->bnet_win_rect.bottom - g_ddraw->bnet_win_rect.top;

            int x = g_ddraw->bnet_pos.x || g_ddraw->bnet_pos.y ? g_ddraw->bnet_pos.x : -32000;
            int y = g_ddraw->bnet_pos.x || g_ddraw->bnet_pos.y ? g_ddraw->bnet_pos.y : -32000;

            UINT flags = width != g_ddraw->width || height != g_ddraw->height ? 0 : SWP_NOMOVE;

            int dst_width = g_config.window_rect.right ? g_ddraw->width : 0;
            int dst_height = g_config.window_rect.bottom ? g_ddraw->height : 0;

            util_set_window_rect(x, y, dst_width, dst_height, flags);
            g_ddraw->resizable = FALSE;

            g_ddraw->bnet_active = TRUE;
            SetTimer(g_ddraw->hwnd, IDT_TIMER_REDRAW_BNET, 200, (TIMERPROC)NULL);
            mouse_unlock();
            ReleaseCapture();
        }

        POINT pt = { 0, 0 };
        real_ClientToScreen(g_ddraw->hwnd, &pt);

        int added_height = g_ddraw->height - 480;
        int added_width = g_ddraw->width - 640;
        int align_y = added_height > 0 ? added_height / 2 : 0;
        int align_x = added_width > 0 ? added_width / 2 : 0;

        if (g_ddraw->windowed)
        {
            X += align_x;
            Y += align_y;
        }
        else if (GetSystemMetrics(SM_CMONITORS) > 1)
        {
            X += min(g_ddraw->render.width / 2 - 640 / 2, added_width);
            Y += min(g_ddraw->render.height / 2 - 480 / 2, added_height);
        }

        X += pt.x;
        Y += pt.y;

        dwStyle |= WS_CLIPCHILDREN;

        if (lpWindowName && !lpWindowName[0])
        {
            lpWindowName = "Bnet Lobby";
        }
    }

    return real_CreateWindowExA(
        dwExStyle,
        lpClassName,
        lpWindowName,
        dwStyle,
        X,
        Y,
        nWidth,
        nHeight,
        hWndParent,
        hMenu,
        hInstance,
        lpParam);
}

HRESULT WINAPI fake_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv)
{
    if (rclsid && riid && (IsEqualGUID(&CLSID_DirectDraw, rclsid) || IsEqualGUID(&CLSID_DirectDraw7, rclsid)))
    {
        if (IsEqualGUID(&IID_IDirectDraw2, riid) ||
            IsEqualGUID(&IID_IDirectDraw4, riid) || 
            IsEqualGUID(&IID_IDirectDraw7, riid))
        {
            return dd_CreateEx(NULL, ppv, riid, NULL);
        }
        else
        {
            return dd_CreateEx(NULL, ppv, &IID_IDirectDraw, NULL);
        }
    }

    return real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}
