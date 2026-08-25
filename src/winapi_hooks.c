#include <windows.h>
#include <windowsx.h>
#include <math.h>
#include <vfw.h>
#include "debug.h"
#include "config.h"
#include "dd.h"
#include "ddraw.h"
#include "hook.h"
#include "config.h"
#include "utils.h"
#include "mouse.h"
#include "keyboard.h"
#include "wndproc.h"
#include "render_gdi.h"
#include "render_d3d9.h"
#include "render_ogl.h"
#include "directinput.h"
#include "ddsurface.h"
#include "ddclipper.h"
#include "dllmain.h"
#include "hook.h"
#include "directinput.h"
#include "ddpalette.h"
#include "palette.h"


BOOL WINAPI fake_GetCursorPos(LPPOINT lpPoint)
{
    if (!g_ddraw.ref || !g_ddraw.hwnd || !g_ddraw.width)
        return real_GetCursorPos(lpPoint);

    POINT pt, realpt;

    if (!real_GetCursorPos(&pt))
        return FALSE;

    realpt = pt;

    if ((g_mouse_locked || g_config.devmode || g_ddraw.bnet_active) && 
        (!g_config.windowed || real_ScreenToClient(g_ddraw.hwnd, &pt)))
    {
        int x = max(pt.x - g_ddraw.mouse.x_adjust, 0);
        int y = max(pt.y - g_ddraw.mouse.y_adjust, 0);

        if (g_config.adjmouse && !g_ddraw.child_window_exists)
        {
            x = (DWORD)(roundf(x * g_ddraw.mouse.unscale_x));
            y = (DWORD)(roundf(y * g_ddraw.mouse.unscale_y));
        }

        x = min(x, g_ddraw.width - 1);
        y = min(y, g_ddraw.height - 1);

        if (g_config.vhack && 
            !g_ddraw.isworms2 && 
            !g_config.devmode && 
            !g_ddraw.bnet_active && 
            InterlockedExchangeAdd(&g_ddraw.upscale_hack_active, 0))
        {
            int diffx = 0;
            int diffy = 0;

            if (x > g_ddraw.upscale_hack_width)
            {
                diffx = x - g_ddraw.upscale_hack_width;
                x = g_ddraw.upscale_hack_width;
            }

            if (y > g_ddraw.upscale_hack_height)
            {
                diffy = y - g_ddraw.upscale_hack_height;
                y = g_ddraw.upscale_hack_height;
            }

            if (diffx || diffy)
                real_SetCursorPos(realpt.x - diffx, realpt.y - diffy);
        }

        InterlockedExchange((LONG*)&g_ddraw.cursor.x, x);
        InterlockedExchange((LONG*)&g_ddraw.cursor.y, y);

        if (lpPoint)
        {
            lpPoint->x = x;
            lpPoint->y = y;
        }

        return TRUE;
    }

    if (lpPoint)
    {
        lpPoint->x = InterlockedExchangeAdd((LONG*)&g_ddraw.cursor.x, 0);
        lpPoint->y = InterlockedExchangeAdd((LONG*)&g_ddraw.cursor.y, 0);
    }

    return TRUE;
}

BOOL WINAPI fake_ClipCursor(const RECT* lpRect)
{
    if (g_ddraw.ref && g_ddraw.hwnd && g_ddraw.width)
    {
        RECT dst_rc = {
            0,
            0,
            g_ddraw.width,
            g_ddraw.height
        };

        if (lpRect)
            CopyRect(&dst_rc, lpRect);

        if (g_config.adjmouse)
        {
            dst_rc.left = (LONG)(roundf(dst_rc.left * g_ddraw.render.scale_w));
            dst_rc.top = (LONG)(roundf(dst_rc.top * g_ddraw.render.scale_h));
            dst_rc.bottom = (LONG)(roundf(dst_rc.bottom * g_ddraw.render.scale_h));
            dst_rc.right = (LONG)(roundf(dst_rc.right * g_ddraw.render.scale_w));
        }

        int max_width = g_config.adjmouse ? g_ddraw.render.viewport.width : g_ddraw.width;
        int max_height = g_config.adjmouse ? g_ddraw.render.viewport.height : g_ddraw.height;

        dst_rc.bottom = min(dst_rc.bottom, max_height);
        dst_rc.right = min(dst_rc.right, max_width);

        OffsetRect(
            &dst_rc,
            g_ddraw.mouse.x_adjust, 
            g_ddraw.mouse.y_adjust);

        CopyRect(&g_ddraw.mouse.rc, &dst_rc);

        if (g_mouse_locked && !util_is_minimized(g_ddraw.hwnd))
        {
            real_MapWindowPoints(g_ddraw.hwnd, HWND_DESKTOP, (LPPOINT)&dst_rc, 2);

            return real_ClipCursor(&dst_rc);
        }
    }

    return TRUE;
}

int WINAPI fake_ShowCursor(BOOL bShow)
{
    if (g_ddraw.ref && g_ddraw.hwnd)
    {
        if (g_mouse_locked || g_config.devmode)
        {
            int count = real_ShowCursor(bShow);
            InterlockedExchange((LONG*)&g_ddraw.show_cursor_count, count);
            return count;
        }
        else
        {
            return bShow ?
                InterlockedIncrement((LONG*)&g_ddraw.show_cursor_count) :
                InterlockedDecrement((LONG*)&g_ddraw.show_cursor_count);
        }
    }

    return real_ShowCursor(bShow);
}

HCURSOR WINAPI fake_SetCursor(HCURSOR hCursor)
{
    if (g_ddraw.ref && g_ddraw.hwnd)
    {
        HCURSOR cursor = (HCURSOR)InterlockedExchange((LONG*)&g_ddraw.old_cursor, (LONG)hCursor);

        if (!g_mouse_locked && !g_config.devmode)
            return cursor;
    }

    return real_SetCursor(hCursor);
}

BOOL WINAPI fake_GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    if (lpRect &&
        g_ddraw.ref &&
        g_ddraw.hwnd &&
        g_ddraw.width &&
        (g_config.hook != 2 || g_ddraw.renderer == gdi_render_main))
    {
        if (hWnd == g_ddraw.hwnd || hWnd == GetDesktopWindow())
        {
            lpRect->bottom = g_ddraw.height;
            lpRect->left = 0;
            lpRect->right = g_ddraw.width;
            lpRect->top = 0;

            return TRUE;
        }
        else
        {
            if (real_GetWindowRect(hWnd, lpRect))
            {
                real_MapWindowPoints(HWND_DESKTOP, g_ddraw.hwnd, (LPPOINT)lpRect, 2);

                {
                    /* This hook maps ANY window into game-window client space, and that space
                       is offset from surface space by the surface origin. So the inverse must
                       be applied to every window, not just direct children - the game mixes
                       dialog and control rects arithmetically, and transforming only one side
                       leaks the offset into the result. */
                    int ox, oy;
                    util_get_surface_origin(&ox, &oy);
                    lpRect->left -= ox;
                    lpRect->right -= ox;
                    lpRect->top -= oy;
                    lpRect->bottom -= oy;
                }

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
        g_ddraw.ref &&
        g_ddraw.width &&
        (g_ddraw.hwnd == hWnd || hWnd == GetDesktopWindow()) &&
        (g_config.hook != 2 || g_ddraw.renderer == gdi_render_main))
    {
        lpRect->bottom = g_ddraw.height;
        lpRect->left = 0;
        lpRect->right = g_ddraw.width;
        lpRect->top = 0;

        return TRUE;
    }

    return real_GetClientRect(hWnd, lpRect);
}

BOOL WINAPI fake_ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    if (!g_ddraw.ref || !g_ddraw.hwnd)
        return real_ClientToScreen(hWnd, lpPoint);

    if (g_ddraw.hwnd != hWnd)
    {
        if (real_ClientToScreen(hWnd, lpPoint) && real_ScreenToClient(g_ddraw.hwnd, lpPoint))
        {
            /* window client space -> surface space */
            int ox, oy;
            util_get_surface_origin(&ox, &oy);
            lpPoint->x -= ox;
            lpPoint->y -= oy;
            return TRUE;
        }

        return FALSE;
    }

    return TRUE;
}

BOOL WINAPI fake_ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    if (!g_ddraw.ref || !g_ddraw.hwnd)
        return real_ScreenToClient(hWnd, lpPoint);

    if (g_ddraw.hwnd != hWnd)
    {
        /* surface space -> window client space */
        int ox, oy;
        util_get_surface_origin(&ox, &oy);
        lpPoint->x += ox;
        lpPoint->y += oy;

        return real_ClientToScreen(g_ddraw.hwnd, lpPoint) && real_ScreenToClient(hWnd, lpPoint);
    }

    return TRUE;
}

BOOL WINAPI fake_SetCursorPos(int X, int Y)
{
    if (!g_ddraw.ref || !g_ddraw.hwnd || !g_ddraw.width)
        return real_SetCursorPos(X, Y);

    if (!g_mouse_locked && !g_config.devmode)
        return TRUE;

    POINT pt = { X, Y };

    if (g_config.adjmouse)
    {
        pt.x = (LONG)(roundf(pt.x * g_ddraw.mouse.scale_x));
        pt.y = (LONG)(roundf(pt.y * g_ddraw.mouse.scale_y));
    }

    pt.x += g_ddraw.mouse.x_adjust;
    pt.y += g_ddraw.mouse.y_adjust;

    return real_ClientToScreen(g_ddraw.hwnd, &pt) && real_SetCursorPos(pt.x, pt.y);
}

HWND WINAPI fake_WindowFromPoint(POINT Point)
{
    if (!g_ddraw.ref || !g_ddraw.hwnd)
        return real_WindowFromPoint(Point);

    POINT pt = { Point.x, Point.y };
    return real_ClientToScreen(g_ddraw.hwnd, &pt) ? real_WindowFromPoint(pt) : NULL;
}

BOOL WINAPI fake_GetClipCursor(LPRECT lpRect)
{
    if (!g_ddraw.ref || !g_ddraw.width)
        return real_GetClipCursor(lpRect);

    if (lpRect)
    {
        lpRect->bottom = g_ddraw.height;
        lpRect->left = 0;
        lpRect->right = g_ddraw.width;
        lpRect->top = 0;

        return TRUE;
    }

    return FALSE;
}

BOOL WINAPI fake_GetCursorInfo(PCURSORINFO pci)
{
    if (!g_ddraw.ref || !g_ddraw.hwnd)
        return real_GetCursorInfo(pci);

    return pci && real_GetCursorInfo(pci) && real_ScreenToClient(g_ddraw.hwnd, &pci->ptScreenPos);
}

int WINAPI fake_GetSystemMetrics(int nIndex)
{
    DWORD width = 0;
    DWORD height = 0;

    if (g_ddraw.ref && g_ddraw.width)
    {
        width = g_ddraw.width;
        height = g_ddraw.height;
    }
    else if (g_config.fake_mode[0])
    {
        char* e = &g_config.fake_mode[0];

        width = strtoul(e, &e, 0);
        height = strtoul(e + 1, &e, 0);
    }

    if (width)
    {
        if (nIndex == SM_CXSCREEN)
            return width;

        if (nIndex == SM_CYSCREEN)
            return height;
    }

    return real_GetSystemMetrics(nIndex);
}

BOOL WINAPI fake_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    if (g_ddraw.ref && g_ddraw.hwnd)
    {
        if (g_ddraw.hwnd == hWnd)
        {
            UINT req_flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER;

            if ((uFlags & req_flags) != req_flags)
                return TRUE;
        }
        else if (!IsChild(g_ddraw.hwnd, hWnd) && !(real_GetWindowLongA(hWnd, GWL_STYLE) & WS_CHILD))
        {
            POINT pt = { 0, 0 };
            if (real_ClientToScreen(g_ddraw.hwnd, &pt))
            {
                X += pt.x;
                Y += pt.y;
            }
        }
        else if (!(uFlags & SWP_NOMOVE) && GetParent(hWnd) == g_ddraw.hwnd)
        {
            /* The game positions its child windows in surface space. Shift them by the
               surface origin so they line up with where the surface is actually drawn.
               Direct children only - grandchildren are relative to their own parent. */
            int ox, oy;
            util_get_surface_origin(&ox, &oy);
            X += ox;
            Y += oy;
        }
    }

    return real_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

BOOL WINAPI fake_MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    if (g_ddraw.ref && g_ddraw.hwnd)
    {
        if (g_ddraw.hwnd == hWnd)
        {
            if (g_ddraw.width && g_ddraw.height && (nWidth != g_ddraw.width || nHeight != g_ddraw.height))
            {
                //real_SendMessageA(g_ddraw.hwnd, WM_MOVE_DDRAW, 0, MAKELPARAM(X, Y));

                real_SendMessageA(
                    g_ddraw.hwnd,
                    WM_SIZE_DDRAW,
                    0,
                    MAKELPARAM(min(nWidth, g_ddraw.width), min(nHeight, g_ddraw.height)));
            }

            return TRUE;
        }
        else if (!IsChild(g_ddraw.hwnd, hWnd) && !(real_GetWindowLongA(hWnd, GWL_STYLE) & WS_CHILD))
        {
            POINT pt = { 0, 0 };
            if (real_ClientToScreen(g_ddraw.hwnd, &pt))
            {
                X += pt.x;
                Y += pt.y;
            }
        }
        else if (hWnd == g_ddraw.textbox.hwnd && IsWindow(hWnd) && GetParent(hWnd) == g_ddraw.hwnd && g_ddraw.width)
        {
            /* Age Of Empires 2 textbox align */
            char class_name[MAX_PATH] = { 0 };
            GetClassNameA(hWnd, class_name, sizeof(class_name) - 1);

            if (_strcmpi(class_name, "Edit") == 0)
            {
                g_ddraw.textbox.x = X;
                g_ddraw.textbox.y = Y;

                X = (int)(g_ddraw.render.viewport.x + (X * g_ddraw.render.scale_w));
                Y = (int)(g_ddraw.render.viewport.y + (Y * g_ddraw.render.scale_h));
            }
        }
        else if (GetParent(hWnd) == g_ddraw.hwnd)
        {
            /* Same idea as the AoE2 textbox above, generalised: the game positions its child
               windows in surface space, so shift them by the surface origin to match where the
               surface is drawn. Origin only - the surface is rendered unscaled while a child
               window is present. Direct children only. */
            int ox, oy;
            util_get_surface_origin(&ox, &oy);
            X += ox;
            Y += oy;
        }
    }

    return real_MoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
}

LRESULT WINAPI fake_SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (!g_ddraw.ref || !g_ddraw.hwnd)
        return real_SendMessageA(hWnd, Msg, wParam, lParam);

    if (g_ddraw.hwnd == hWnd && Msg == WM_MOUSEMOVE)
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (g_config.adjmouse)
        {
            x = (int)(roundf(x * g_ddraw.mouse.scale_x));
            y = (int)(roundf(y * g_ddraw.mouse.scale_y));
        }

        lParam = MAKELPARAM(x + g_ddraw.mouse.x_adjust, y + g_ddraw.mouse.y_adjust);
    }

    if (g_ddraw.hwnd == hWnd && Msg == WM_SIZE && g_config.hook != 2)
    {
        Msg = WM_SIZE_DDRAW;
    }

    LRESULT result = real_SendMessageA(hWnd, Msg, wParam, lParam);

    if (result && g_ddraw.ref && Msg == CB_GETDROPPEDCONTROLRECT)
    {
        RECT* rc = (RECT*)lParam;
        if (rc)
            real_MapWindowPoints(HWND_DESKTOP, g_ddraw.hwnd, (LPPOINT)rc, 2);
    }

    return result;
}

LONG WINAPI fake_SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
    if (g_ddraw.ref && g_ddraw.hwnd && g_ddraw.hwnd == hWnd)
    {
        if (nIndex == GWL_STYLE)
            return 0;

        if (nIndex == GWL_WNDPROC)
        {
            WNDPROC old = g_ddraw.wndproc;
            g_ddraw.wndproc = (WNDPROC)dwNewLong;

            return (LONG)old;        
        }
    }

    return real_SetWindowLongA(hWnd, nIndex, dwNewLong);
}

LONG WINAPI fake_SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong)
{
    if (g_ddraw.ref && g_ddraw.hwnd && g_ddraw.hwnd == hWnd)
    {
        if (nIndex == GWL_STYLE)
            return 0;
    }

    return real_SetWindowLongW(hWnd, nIndex, dwNewLong);
}

LONG WINAPI fake_GetWindowLongA(HWND hWnd, int nIndex)
{
    if (g_ddraw.ref && g_ddraw.hwnd == hWnd)
    {
        if (nIndex == GWL_WNDPROC)
        {
            return (LONG)g_ddraw.wndproc;
        }
    }

    return real_GetWindowLongA(hWnd, nIndex);
}

BOOL WINAPI fake_EnableWindow(HWND hWnd, BOOL bEnable)
{
    if (g_ddraw.ref && g_ddraw.hwnd == hWnd)
    {
        return FALSE;
    }

    return real_EnableWindow(hWnd, bEnable);
}

int WINAPI fake_MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints)
{
    if (g_ddraw.ref && g_ddraw.hwnd)
    {
        int ox, oy;
        UINT i;

        util_get_surface_origin(&ox, &oy);

        if (hWndTo == HWND_DESKTOP)
        {
            if (hWndFrom == g_ddraw.hwnd)
            {
                return 0;
            }
            else
            {
                int ret;

                real_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);
                ret = real_MapWindowPoints(HWND_DESKTOP, g_ddraw.hwnd, lpPoints, cPoints);

                /* window client space -> surface space */
                for (i = 0; lpPoints && i < cPoints; i++)
                {
                    lpPoints[i].x -= ox;
                    lpPoints[i].y -= oy;
                }

                return ret;
            }
        }

        if (hWndFrom == HWND_DESKTOP)
        {
            if (hWndTo == g_ddraw.hwnd)
            {
                return 0;
            }
            else
            {
                /* surface space -> window client space */
                for (i = 0; lpPoints && i < cPoints; i++)
                {
                    lpPoints[i].x += ox;
                    lpPoints[i].y += oy;
                }

                real_MapWindowPoints(g_ddraw.hwnd, HWND_DESKTOP, lpPoints, cPoints);
                return real_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);
            }
        }
    }

    return real_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);
}

BOOL WINAPI fake_ShowWindow(HWND hWnd, int nCmdShow)
{
    /* Don't hide the taskbar (Some of The Learning Company games) */
    if (nCmdShow == SW_HIDE && hWnd && hWnd != g_ddraw.hwnd && hWnd == FindWindowA("Shell_TrayWnd", NULL))
    {
        return TRUE;
    }

    if (g_ddraw.ref && g_ddraw.hwnd == hWnd)
    {
        /* Make sure we got close/move menu items (Almost all of the The Learning Company games) */
        HMENU menu = GetSystemMenu(hWnd, FALSE);
        if (!menu || GetMenuState(menu, SC_CLOSE, MF_BYCOMMAND) == -1 || GetMenuState(menu, SC_MOVE, MF_BYCOMMAND) == -1)
        {
            GetSystemMenu(hWnd, TRUE);
        }

        if (nCmdShow == SW_SHOWMAXIMIZED)
            nCmdShow = SW_SHOWNORMAL;

        if (nCmdShow == SW_MAXIMIZE)
            nCmdShow = SW_NORMAL;

        if (nCmdShow == SW_MINIMIZE && g_config.hook != 2 && !g_config.tlc_hack)
            return TRUE;
    }

    return real_ShowWindow(hWnd, nCmdShow);
}

HWND WINAPI fake_GetTopWindow(HWND hWnd)
{
    if (g_ddraw.ref && g_config.windowed && g_ddraw.hwnd && !hWnd)
    {
        return g_ddraw.hwnd;
    }

    return real_GetTopWindow(hWnd);
}

HWND WINAPI fake_GetForegroundWindow()
{
    if (g_ddraw.ref && g_config.windowed && g_ddraw.hwnd)
    {
        return g_ddraw.hwnd;
    }

    return real_GetForegroundWindow();
}

BOOL WINAPI fake_SetForegroundWindow(HWND hWnd)
{
    if (g_ddraw.ref && g_ddraw.bnet_active)
    {
        return TRUE;
    }

    return real_SetForegroundWindow(hWnd);
}

HHOOK WINAPI fake_SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId)
{
    TRACE(
        "SetWindowsHookExA(idHook=%d, lpfn=%p, hmod=%p, dwThreadId=%d) [%p]\n",
        idHook,
        lpfn, 
        hmod, 
        dwThreadId, 
        _ReturnAddress());

    dbg_dump_hook_type(idHook);

    if (idHook == WH_KEYBOARD_LL && hmod && GetModuleHandle("AcGenral") == hmod)
    {
        return NULL;
    }

    if (idHook == WH_MOUSE && lpfn && !hmod && !g_mouse_hook && g_config.sirtech_hack)
    {
        g_mouse_proc = lpfn;
        return g_mouse_hook = real_SetWindowsHookExA(idHook, mouse_hook_proc, hmod, dwThreadId);
    }

    HHOOK result = real_SetWindowsHookExA(idHook, lpfn, hmod, dwThreadId);

    if (idHook == WH_KEYBOARD)
    {
        keyboard_hook_init();
    }

    return result;
}

void HandleMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    if (lpMsg && g_ddraw.ref && g_ddraw.hwnd && g_ddraw.width)
    {
        if (!g_config.windowed || real_ScreenToClient(g_ddraw.hwnd, &lpMsg->pt))
        {
            int x = max(lpMsg->pt.x - g_ddraw.mouse.x_adjust, 0);
            int y = max(lpMsg->pt.y - g_ddraw.mouse.y_adjust, 0);

            if (g_config.adjmouse)
            {
                x = (DWORD)(roundf(x * g_ddraw.mouse.unscale_x));
                y = (DWORD)(roundf(y * g_ddraw.mouse.unscale_y));
            }

            lpMsg->pt.x = min(x, g_ddraw.width - 1);
            lpMsg->pt.y = min(y, g_ddraw.height - 1);
        }
        else
        {
            lpMsg->pt.x = InterlockedExchangeAdd((LONG*)&g_ddraw.cursor.x, 0);
            lpMsg->pt.y = InterlockedExchangeAdd((LONG*)&g_ddraw.cursor.y, 0);
        }

        if (lpMsg->hwnd != g_ddraw.hwnd || !g_config.hook_peekmessage)
            return;

        switch (LOWORD(lpMsg->message))
        {
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        {
            if (!g_config.devmode && !g_mouse_locked && (wRemoveMsg & PM_REMOVE))
            {
                int x = GET_X_LPARAM(lpMsg->lParam);
                int y = GET_Y_LPARAM(lpMsg->lParam);

                if (x > g_ddraw.render.viewport.x + g_ddraw.render.viewport.width ||
                    x < g_ddraw.render.viewport.x ||
                    y > g_ddraw.render.viewport.y + g_ddraw.render.viewport.height ||
                    y < g_ddraw.render.viewport.y)
                {
                    x = g_ddraw.width / 2;
                    y = g_ddraw.height / 2;
                }
                else
                {
                    x = (DWORD)((x - g_ddraw.render.viewport.x) * g_ddraw.mouse.unscale_x);
                    y = (DWORD)((y - g_ddraw.render.viewport.y) * g_ddraw.mouse.unscale_y);
                }

                InterlockedExchange((LONG*)&g_ddraw.cursor.x, x);
                InterlockedExchange((LONG*)&g_ddraw.cursor.y, y);

                mouse_lock();

                if (!wMsgFilterMin &&
                    !wMsgFilterMax &&
                    !(wRemoveMsg & (PM_QS_INPUT | PM_QS_PAINT | PM_QS_POSTMESSAGE | PM_QS_SENDMESSAGE)))
                {
                    lpMsg->message = WM_NULL;
                    break;
                }
            }
            /* fall through for lParam */
        }
        /* down messages are ignored if we have no cursor lock */
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHOVER:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_MOUSEMOVE:
        {
            if (!g_config.devmode && !g_mouse_locked)
            {
                if (!wMsgFilterMin &&
                    !wMsgFilterMax &&
                    !(wRemoveMsg & (PM_QS_INPUT | PM_QS_PAINT | PM_QS_POSTMESSAGE | PM_QS_SENDMESSAGE)))
                {
                    lpMsg->message = WM_NULL;
                    break;
                }
            }

            if (LOWORD(lpMsg->message) == WM_MOUSEWHEEL)
            {
                POINT pt = { GET_X_LPARAM(lpMsg->lParam), GET_Y_LPARAM(lpMsg->lParam) };
                real_ScreenToClient(g_ddraw.hwnd, &pt);
                lpMsg->lParam = MAKELPARAM(pt.x, pt.y);
            }

            int x = max(GET_X_LPARAM(lpMsg->lParam) - g_ddraw.mouse.x_adjust, 0);
            int y = max(GET_Y_LPARAM(lpMsg->lParam) - g_ddraw.mouse.y_adjust, 0);

            if (g_config.adjmouse)
            {
                if (g_config.vhack && !g_config.devmode)
                {
                    POINT pt = { 0, 0 };
                    fake_GetCursorPos(&pt);

                    x = pt.x;
                    y = pt.y;
                }
                else
                {
                    x = (DWORD)(roundf(x * g_ddraw.mouse.unscale_x));
                    y = (DWORD)(roundf(y * g_ddraw.mouse.unscale_y));
                }
            }

            x = min(x, g_ddraw.width - 1);
            y = min(y, g_ddraw.height - 1);

            InterlockedExchange((LONG*)&g_ddraw.cursor.x, x);
            InterlockedExchange((LONG*)&g_ddraw.cursor.y, y);

            lpMsg->lParam = MAKELPARAM(x, y);

            lpMsg->pt.x = x;
            lpMsg->pt.y = y;

            break;
        }
        }
    }
}

BOOL WINAPI fake_GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    if (g_ddraw.ref && (!hWnd || hWnd == g_ddraw.hwnd))
        g_ddraw.last_msg_pull_tick = timeGetTime();

    BOOL result = real_GetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
    if (result)
    {
        HandleMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE);
    }

    return result;
}

BOOL WINAPI fake_PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    if (g_config.darkcolony_hack && !hWnd)
    {
        hWnd = g_ddraw.hwnd;

        MSG msg;
        real_PeekMessageA(&msg, 0, 0, 0, PM_NOREMOVE);
    }

    if (g_config.limiter_type == LIMIT_PEEKMESSAGE && 
        g_ddraw.ticks_limiter.tick_length > 0 &&
        InterlockedExchange(&g_ddraw.render.screen_updated, FALSE))
    {
        util_limit_game_ticks();
    }

    if (g_ddraw.ref && (!hWnd || hWnd == g_ddraw.hwnd))
        g_ddraw.last_msg_pull_tick = timeGetTime();

    BOOL result = real_PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    if (result)
    {
        HandleMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    }

    return result;
}

BOOL WINAPI fake_GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT* lpwndpl)
{
    BOOL result = real_GetWindowPlacement(hWnd, lpwndpl);

    if (result &&
        lpwndpl &&
        g_ddraw.ref &&
        g_ddraw.hwnd &&
        g_ddraw.width &&
        (g_config.hook != 2 || g_ddraw.renderer == gdi_render_main))
    {
        if (hWnd == g_ddraw.hwnd || hWnd == GetDesktopWindow())
        {
            lpwndpl->rcNormalPosition.bottom = g_ddraw.height;
            lpwndpl->rcNormalPosition.left = 0;
            lpwndpl->rcNormalPosition.right = g_ddraw.width;
            lpwndpl->rcNormalPosition.top = 0;
        }
        else if (!IsChild(g_ddraw.hwnd, hWnd) && !(real_GetWindowLongA(hWnd, GWL_STYLE) & WS_CHILD))
        {
            real_MapWindowPoints(HWND_DESKTOP, g_ddraw.hwnd, (LPPOINT)&lpwndpl->rcNormalPosition, 2);
        }
    }

    return result;
}

BOOL WINAPI fake_SetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT* lpwndpl)
{
    if (lpwndpl && g_ddraw.ref && g_ddraw.hwnd && hWnd == g_ddraw.hwnd)
    {
        if (lpwndpl->showCmd == SW_SHOWMAXIMIZED || lpwndpl->showCmd == SW_MAXIMIZE)
            return TRUE;
    }

    return real_SetWindowPlacement(hWnd, lpwndpl);
}

BOOL WINAPI fake_EnumDisplaySettingsA(LPCSTR lpszDeviceName, DWORD iModeNum, DEVMODEA* lpDevMode)
{
    BOOL result = real_EnumDisplaySettingsA(lpszDeviceName, iModeNum, lpDevMode);

    if (result && !lpszDeviceName && lpDevMode && iModeNum == ENUM_CURRENT_SETTINGS)
    {
        if (g_ddraw.ref && g_ddraw.width)
        {
            lpDevMode->dmPelsWidth = g_ddraw.width;
            lpDevMode->dmPelsHeight = g_ddraw.height;
            lpDevMode->dmBitsPerPel = g_ddraw.bpp;
        }
        else if (g_config.fake_mode[0])
        {
            char* e = &g_config.fake_mode[0];

            lpDevMode->dmPelsWidth = strtoul(e, &e, 0);
            lpDevMode->dmPelsHeight = strtoul(e + 1, &e, 0);
            lpDevMode->dmBitsPerPel = strtoul(e + 1, &e, 0);
        }
        else
        {
            lpDevMode->dmPelsWidth = 1024;
            lpDevMode->dmPelsHeight = 768;
            lpDevMode->dmBitsPerPel = 16;
        }

        lpDevMode->dmDisplayFrequency = 60;
    }

    if (result && !lpszDeviceName && lpDevMode && iModeNum != ENUM_CURRENT_SETTINGS)
    {
        //lpDevMode->dmBitsPerPel = 16;
    }

    return result;
}

LRESULT WINAPI fake_DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (g_ddraw.ref && g_ddraw.hwnd && g_ddraw.hwnd == hWnd)
    {
        if (Msg == WM_NCHITTEST)
            return HTCLIENT;
    }

    return real_DefWindowProcA(hWnd, Msg, wParam, lParam);
}

HWND WINAPI fake_SetParent(HWND hWndChild, HWND hWndNewParent)
{
    if (g_ddraw.ref && g_ddraw.hwnd && g_ddraw.hwnd == hWndNewParent)
    {
        char class_name[MAX_PATH] = { 0 };
        GetClassNameA(hWndChild, class_name, sizeof(class_name) - 1);

        if (strcmp(class_name, "VideoRenderer") == 0)
        {
            RECT rc_org;
            fake_GetWindowRect(hWndChild, &rc_org);
 
            HWND result = real_SetParent(hWndChild, hWndNewParent);

            real_MoveWindow(
                hWndChild,
                rc_org.left,
                rc_org.top,
                (rc_org.right - rc_org.left),
                (rc_org.bottom - rc_org.top),
                FALSE);

            return result;
        }
    }

    return real_SetParent(hWndChild, hWndNewParent);
}

HDC WINAPI fake_BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint)
{
    if (g_ddraw.ref && g_ddraw.width && g_ddraw.hwnd && g_ddraw.hwnd == hWnd && lpPaint)
    {
        HDC result = real_BeginPaint(hWnd, lpPaint);

        if (result)
        {
            lpPaint->rcPaint.left = 0;
            lpPaint->rcPaint.top = 0;
            lpPaint->rcPaint.right = g_ddraw.width;
            lpPaint->rcPaint.bottom = g_ddraw.height;
        }

        return result;
    }

    return real_BeginPaint(hWnd, lpPaint);
}

SHORT WINAPI fake_GetKeyState(int nVirtKey)
{
    if (g_config.windowed && g_ddraw.ref && g_ddraw.hwnd && !util_in_foreground())
    {
        return 0;
    }

    return real_GetKeyState(nVirtKey);
}

SHORT WINAPI fake_GetAsyncKeyState(int vKey)
{
    if (g_config.windowed && g_ddraw.ref && g_ddraw.hwnd && !util_in_foreground())
    {
        return 0;
    }

    return real_GetAsyncKeyState(vKey);
}

int WINAPI fake_GetDeviceCaps(HDC hdc, int index)
{
    DWORD bpp = 0;
    DWORD width = 0;
    DWORD height = 0;

    if (g_ddraw.ref && g_ddraw.bpp)
    {
        bpp = g_ddraw.bpp;
        width = g_ddraw.width;
        height = g_ddraw.height;
    }
    else if (g_config.fake_mode[0])
    {
        char* e = &g_config.fake_mode[0];

        width = strtoul(e, &e, 0);
        height = strtoul(e + 1, &e, 0);
        bpp = strtoul(e + 1, &e, 0);
    }

    if (bpp && index == BITSPIXEL)
    {
        return bpp;
    }

    if (bpp == 8)
    {
        if (index == RASTERCAPS)
        {
            return RC_PALETTE | real_GetDeviceCaps(hdc, index);
        }

        if (index == SIZEPALETTE)
        {
            return 256;
        }

        if (index == NUMCOLORS)
        {
            if (WindowFromDC(hdc) == GetDesktopWindow())
            {
                return 20;
            }
            else
            {
                return 256;
            }
        }
    }

    if (width && ((g_ddraw.hwnd && WindowFromDC(hdc) == g_ddraw.hwnd) || WindowFromDC(hdc) == GetDesktopWindow()))
    {
        if (index == HORZRES)
        {
            return width;
        }

        if (index == VERTRES)
        {
            return height;
        }
    }

    return real_GetDeviceCaps(hdc, index);
}

int WINAPI fake_GetDeviceCaps_system(HDC hdc, int index)
{
    if (g_ddraw.ref &&
        g_ddraw.bpp == 8 &&
        ((g_ddraw.hwnd && WindowFromDC(hdc) == g_ddraw.hwnd) || WindowFromDC(hdc) == GetDesktopWindow()))
    {
        if (index == RASTERCAPS)
        {
            return RC_PALETTE | real_GetDeviceCaps(hdc, index);
        }
    }

    return real_GetDeviceCaps(hdc, index);
}

BOOL WINAPI fake_StretchBlt(
    HDC hdcDest,
    int xDest,
    int yDest,
    int wDest,
    int hDest,
    HDC hdcSrc,
    int xSrc,
    int ySrc,
    int wSrc,
    int hSrc,
    DWORD rop)
{
    HWND hwnd = WindowFromDC(hdcDest);

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd == g_ddraw.hwnd && !g_ddraw.primary)
    {
        InterlockedExchange(&g_ddraw.render.screen_updated, TRUE);
    }

    char class_name[MAX_PATH] = { 0 };

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd && hwnd != g_ddraw.hwnd)
    {
        GetClassNameA(hwnd, class_name, sizeof(class_name) - 1);
    }

    if (g_ddraw.ref && g_ddraw.hwnd &&
        (hwnd == g_ddraw.hwnd ||
            (g_config.fixchilds && IsChild(g_ddraw.hwnd, hwnd) &&
                (g_config.fixchilds == FIX_CHILDS_DETECT_HIDE ||
                    strcmp(class_name, "VideoRenderer") == 0 ||
                    strcmp(class_name, "AVI Window") == 0 ||
                    strcmp(class_name, "MCIAVI") == 0 ||
                    strcmp(class_name, "AVIWnd32") == 0 ||
                    strcmp(class_name, "MCIWndClass") == 0))))
    {
        if (g_ddraw.primary && (g_ddraw.primary->bpp == 16 || g_ddraw.primary->bpp == 32 || g_ddraw.primary->palette))
        {
            HDC primary_dc;
            dds_GetDC(g_ddraw.primary, &primary_dc);

            if (primary_dc)
            {
                POINT pt = { 0 };
                real_MapWindowPoints(hwnd, g_ddraw.hwnd, &pt, 1);

                int org_mode = SetStretchBltMode(hdcDest, COLORONCOLOR);
                SetStretchBltMode(hdcDest, org_mode);

                int mode = SetStretchBltMode(primary_dc, org_mode);

                BOOL result =
                    real_StretchBlt(
                        primary_dc, 
                        xDest + pt.x,
                        yDest + pt.y, 
                        wDest, 
                        hDest, 
                        hdcSrc, 
                        xSrc, 
                        ySrc, 
                        wSrc, 
                        hSrc, 
                        rop);

                SetStretchBltMode(primary_dc, mode);

                dds_ReleaseDC(g_ddraw.primary, primary_dc);

                return result;
            }
        }
        else if (
            g_ddraw.width > 0 && 
            g_ddraw.render.hdc && 
            (hwnd == g_ddraw.hwnd || 
                (real_GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT) || 
                strcmp(class_name, "AVIWnd32") == 0))
        {
            POINT pt = { 0 };
            real_MapWindowPoints(hwnd, g_ddraw.hwnd, &pt, 1);

            if (hwnd != g_ddraw.hwnd && strcmp(class_name, "AVIWnd32") == 0)
            {
                LONG exstyle = real_GetWindowLongA(hwnd, GWL_EXSTYLE);
                if (!(exstyle & WS_EX_TRANSPARENT))
                {
                    real_SetWindowLongA(hwnd, GWL_EXSTYLE, exstyle | WS_EX_TRANSPARENT);

                    real_SetWindowPos(
                        hwnd,
                        0,
                        0,
                        0,
                        0,
                        0,
                        SWP_ASYNCWINDOWPOS | SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER
                    );
                }
            }

            return real_StretchBlt(
                hwnd == g_ddraw.hwnd ? hdcDest : g_ddraw.render.hdc,
                (int)(roundf((xDest + pt.x) * g_ddraw.render.scale_w)) + g_ddraw.render.viewport.x,
                (int)(roundf((yDest + pt.y) * g_ddraw.render.scale_h)) + g_ddraw.render.viewport.y,
                (int)(roundf(wDest * g_ddraw.render.scale_w)),
                (int)(roundf(hDest * g_ddraw.render.scale_h)),
                hdcSrc,
                xSrc,
                ySrc,
                wSrc,
                hSrc,
                rop);
        }
    }

    return real_StretchBlt(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, rop);
}

BOOL WINAPI fake_WinGStretchBlt(
    HDC hdcDest,
    int xDest,
    int yDest,
    int wDest,
    int hDest,
    HDC hdcSrc,
    int xSrc,
    int ySrc,
    int wSrc,
    int hSrc)
{
    int mode = SetStretchBltMode(hdcDest, COLORONCOLOR);
    BOOL result = fake_StretchBlt(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, SRCCOPY);
    SetStretchBltMode(hdcDest, mode);

    return result;
}

BOOL WINAPI fake_BitBlt(
    HDC hdc, 
    int x, 
    int y, 
    int cx, 
    int cy, 
    HDC hdcSrc, 
    int x1, 
    int y1, 
    DWORD rop)
{
    HWND hwnd = WindowFromDC(hdc);

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd == g_ddraw.hwnd && !g_ddraw.primary)
    {
        InterlockedExchange(&g_ddraw.render.screen_updated, TRUE);
    }

    char class_name[MAX_PATH] = { 0 };

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd && hwnd != g_ddraw.hwnd)
    {
        GetClassNameA(hwnd, class_name, sizeof(class_name) - 1);
    }

    if (g_ddraw.ref && g_ddraw.hwnd &&
        (hwnd == g_ddraw.hwnd ||
            (g_config.fixchilds && IsChild(g_ddraw.hwnd, hwnd) &&
                (g_config.fixchilds == FIX_CHILDS_DETECT_HIDE ||
                    strcmp(class_name, "VideoRenderer") == 0 ||
                    strcmp(class_name, "AVI Window") == 0 ||
                    strcmp(class_name, "MCIAVI") == 0 ||
                    strcmp(class_name, "AVIWnd32") == 0 ||
                    strcmp(class_name, "MCIWndClass") == 0))))
    {
        if (g_ddraw.primary && (g_ddraw.primary->bpp == 16 || g_ddraw.primary->bpp == 32 || g_ddraw.primary->palette))
        {
            HDC primary_dc;
            dds_GetDC(g_ddraw.primary, &primary_dc);

            if (primary_dc)
            {
                POINT pt = { 0 };
                real_MapWindowPoints(hwnd, g_ddraw.hwnd, &pt, 1);

                int result =
                    real_BitBlt(
                        primary_dc,
                        x + pt.x,
                        y + pt.y,
                        cx,
                        cy,
                        hdcSrc,
                        x1,
                        y1,
                        rop);

                dds_ReleaseDC(g_ddraw.primary, primary_dc);

                return result;
            }
        }
        else if (
            g_ddraw.width > 0 &&
            g_ddraw.render.hdc &&
            (hwnd == g_ddraw.hwnd || (real_GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT)))
        {
            return real_StretchBlt(
                hwnd == g_ddraw.hwnd ? hdc : g_ddraw.render.hdc,
                (int)(roundf(x * g_ddraw.render.scale_w)) + g_ddraw.render.viewport.x,
                (int)(roundf(y * g_ddraw.render.scale_h)) + g_ddraw.render.viewport.y,
                (int)(roundf(cx * g_ddraw.render.scale_w)),
                (int)(roundf(cy * g_ddraw.render.scale_h)),
                hdcSrc,
                x1,
                y1,
                cx,
                cy,
                rop);
        }
    }

    return real_BitBlt(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);
}

BOOL WINAPI fake_WinGBitBlt(
    HDC hdc,
    int x,
    int y,
    int cx,
    int cy,
    HDC hdcSrc,
    int x1,
    int y1)
{
    return fake_BitBlt(hdc, x, y, cx, cy, hdcSrc, x1, y1, SRCCOPY);
}

int WINAPI fake_SetDIBitsToDevice(
    HDC hdc,
    int xDest,
    int yDest,
    DWORD w,
    DWORD h,
    int xSrc,
    int ySrc,
    UINT StartScan,
    UINT cLines,
    const VOID* lpvBits,
    const BITMAPINFO* lpbmi,
    UINT ColorUse)
{
    HWND hwnd = WindowFromDC(hdc);

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd == g_ddraw.hwnd && !g_ddraw.primary)
    {
        InterlockedExchange(&g_ddraw.render.screen_updated, TRUE);
    }

    char class_name[MAX_PATH] = { 0 };

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd && hwnd != g_ddraw.hwnd)
    {
        GetClassNameA(hwnd, class_name, sizeof(class_name) - 1);
    }

    if (g_ddraw.ref && g_ddraw.hwnd &&
        (hwnd == g_ddraw.hwnd ||
            (g_config.fixchilds && IsChild(g_ddraw.hwnd, hwnd) &&
                (g_config.fixchilds == FIX_CHILDS_DETECT_HIDE ||
                    strcmp(class_name, "VideoRenderer") == 0 ||
                    strcmp(class_name, "AVI Window") == 0 ||
                    strcmp(class_name, "MCIAVI") == 0 ||
                    strcmp(class_name, "AVIWnd32") == 0 ||
                    strcmp(class_name, "MCIWndClass") == 0))))
    {
        if (g_ddraw.primary && (g_ddraw.primary->bpp == 16 || g_ddraw.primary->bpp == 32 || g_ddraw.primary->palette))
        {
            HDC primary_dc;
            dds_GetDC(g_ddraw.primary, &primary_dc);

            if (primary_dc)
            {
                POINT pt = { 0 };
                real_MapWindowPoints(hwnd, g_ddraw.hwnd, &pt, 1);

                int result =
                    real_SetDIBitsToDevice(
                        primary_dc,
                        xDest + pt.x,
                        yDest + pt.y,
                        w,
                        h,
                        xSrc,
                        ySrc,
                        StartScan,
                        cLines,
                        lpvBits,
                        lpbmi,
                        ColorUse);

                dds_ReleaseDC(g_ddraw.primary, primary_dc);

                return result;
            }
        }
        else if (
            g_ddraw.width > 0 &&
            g_ddraw.render.hdc &&
            (hwnd == g_ddraw.hwnd || (real_GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT)))
        {
            return
                real_StretchDIBits(
                    hwnd == g_ddraw.hwnd ? hdc : g_ddraw.render.hdc,
                    (int)(roundf(xDest * g_ddraw.render.scale_w)) + g_ddraw.render.viewport.x,
                    (int)(roundf(yDest * g_ddraw.render.scale_h)) + g_ddraw.render.viewport.y,
                    (int)(roundf(w * g_ddraw.render.scale_w)),
                    (int)(roundf(h * g_ddraw.render.scale_h)),
                    xSrc,
                    ySrc,
                    w,
                    h,
                    lpvBits,
                    lpbmi,
                    ColorUse,
                    SRCCOPY);
        }
    }

    return real_SetDIBitsToDevice(hdc, xDest, yDest, w, h, xSrc, ySrc, StartScan, cLines, lpvBits, lpbmi, ColorUse);
}

int WINAPI fake_StretchDIBits(
    HDC hdc,
    int xDest,
    int yDest,
    int DestWidth,
    int DestHeight,
    int xSrc,
    int ySrc,
    int SrcWidth,
    int SrcHeight,
    const VOID* lpBits,
    const BITMAPINFO* lpbmi,
    UINT iUsage,
    DWORD rop)
{
    HWND hwnd = WindowFromDC(hdc);

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd == g_ddraw.hwnd && !g_ddraw.primary)
    {
        InterlockedExchange(&g_ddraw.render.screen_updated, TRUE);
    }

    char class_name[MAX_PATH] = { 0 };

    if (g_ddraw.ref && g_ddraw.hwnd && hwnd && hwnd != g_ddraw.hwnd)
    {
        GetClassNameA(hwnd, class_name, sizeof(class_name) - 1);
    }

    if (g_ddraw.ref && g_ddraw.hwnd &&
        (hwnd == g_ddraw.hwnd ||
            (g_config.fixchilds && IsChild(g_ddraw.hwnd, hwnd) &&
                (g_config.fixchilds == FIX_CHILDS_DETECT_HIDE ||
                    strcmp(class_name, "VideoRenderer") == 0 ||
                    strcmp(class_name, "AVI Window") == 0 ||
                    strcmp(class_name, "MCIAVI") == 0 ||
                    strcmp(class_name, "AVIWnd32") == 0 ||
                    strcmp(class_name, "MCIWndClass") == 0))))
    {
        if (g_ddraw.primary && (g_ddraw.primary->bpp == 16 || g_ddraw.primary->bpp == 32 || g_ddraw.primary->palette))
        {
            HDC primary_dc;
            dds_GetDC(g_ddraw.primary, &primary_dc);

            if (primary_dc)
            {
                POINT pt = {0};
                real_MapWindowPoints(hwnd, g_ddraw.hwnd, &pt, 1);

                int result =
                    real_StretchDIBits(
                        primary_dc,
                        xDest + pt.x,
                        yDest + pt.y,
                        DestWidth,
                        DestHeight,
                        xSrc,
                        ySrc,
                        SrcWidth,
                        SrcHeight,
                        lpBits,
                        lpbmi,
                        iUsage,
                        rop);

                dds_ReleaseDC(g_ddraw.primary, primary_dc);

                return result;
            }
        }
        else if (
            g_ddraw.width > 0 &&
            g_ddraw.render.hdc &&
            (hwnd == g_ddraw.hwnd || (real_GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT)))
        {
            return
                real_StretchDIBits(
                    hwnd == g_ddraw.hwnd ? hdc : g_ddraw.render.hdc,
                    (int)(roundf(xDest * g_ddraw.render.scale_w)) + g_ddraw.render.viewport.x,
                    (int)(roundf(yDest * g_ddraw.render.scale_h)) + g_ddraw.render.viewport.y,
                    (int)(roundf(DestWidth * g_ddraw.render.scale_w)),
                    (int)(roundf(DestHeight * g_ddraw.render.scale_h)),
                    xSrc,
                    ySrc,
                    SrcWidth,
                    SrcHeight,
                    lpBits,
                    lpbmi,
                    iUsage,
                    rop);
        }
    }

    return 
        real_StretchDIBits(
            hdc, 
            xDest, 
            yDest, 
            DestWidth, 
            DestHeight, 
            xSrc, 
            ySrc, 
            SrcWidth, 
            SrcHeight, 
            lpBits, 
            lpbmi, 
            iUsage, 
            rop);
}

HFONT WINAPI fake_CreateFontIndirectA(CONST LOGFONTA* lplf)
{
    LOGFONTA lf;
    memcpy(&lf, lplf, sizeof(lf));

    if (lf.lfHeight < 0) {
        lf.lfHeight = min(-g_config.min_font_size, lf.lfHeight);
    }
    else {
        lf.lfHeight = max(g_config.min_font_size, lf.lfHeight);
    }

    if (g_config.anti_aliased_fonts_min_size > abs(lf.lfHeight))
        lf.lfQuality = NONANTIALIASED_QUALITY;

    return real_CreateFontIndirectA(&lf);
}

HFONT WINAPI fake_CreateFontA(
    int nHeight, 
    int nWidth, 
    int nEscapement, 
    int nOrientation, 
    int fnWeight,
    DWORD fdwItalic,
    DWORD fdwUnderline,
    DWORD fdwStrikeOut,
    DWORD fdwCharSet,
    DWORD fdwOutputPrecision,
    DWORD fdwClipPrecision,
    DWORD fdwQuality, 
    DWORD fdwPitchAndFamily,
    LPCTSTR lpszFace)
{
    if (nHeight < 0) {
        nHeight = min(-g_config.min_font_size, nHeight);
    }
    else {
        nHeight = max(g_config.min_font_size, nHeight);
    }

    if (g_config.anti_aliased_fonts_min_size > abs(nHeight))
        fdwQuality = NONANTIALIASED_QUALITY;

    return 
        real_CreateFontA(
            nHeight, 
            nWidth, 
            nEscapement, 
            nOrientation, 
            fnWeight,
            fdwItalic, 
            fdwUnderline, 
            fdwStrikeOut, 
            fdwCharSet,
            fdwOutputPrecision, 
            fdwClipPrecision, 
            fdwQuality, 
            fdwPitchAndFamily,
            lpszFace);
}

UINT WINAPI fake_GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries)
{
    TRACE(
        "%s(hdc=%p, iStart=%u, cEntries=%u, pPalEntries=%p) [%p]\n", 
        __FUNCTION__,
        hdc,
        iStart,
        cEntries,
        pPalEntries,
        _ReturnAddress());

    if (g_ddraw.ref && g_ddraw.bpp == 8 && pPalEntries && GetObjectType(hdc) == OBJ_DC)
    {
        TRACE("     Display DC\n");

        if (g_ddraw.primary && g_ddraw.primary->palette)
        {
            ddp_GetEntries(g_ddraw.primary->palette, 0, iStart, cEntries, pPalEntries);
        }
        else
        {
            for (int i = iStart, x = 0; i < iStart + cEntries && i < 256; i++, x++)
            {
                pPalEntries[x] = g_ddp_default_palette[i];
            }
        }

        return cEntries;
    }

    return real_GetSystemPaletteEntries(hdc, iStart, cEntries, pPalEntries);
}

HPALETTE WINAPI fake_SelectPalette(HDC hdc, HPALETTE hPal, BOOL bForceBkgd)
{
    TRACE_EXT(
        "%s(hdc=%p, hPal=%p, bForceBkgd=%d) [%p]\n",
        __FUNCTION__,
        hdc,
        hPal,
        bForceBkgd,
        _ReturnAddress());

    if (g_ddraw.ref && 
        g_ddraw.bpp == 8 && 
        ((g_ddraw.hwnd && WindowFromDC(hdc) == g_ddraw.hwnd) || WindowFromDC(hdc) == GetDesktopWindow()))
    {
        TRACE_EXT("     Display DC\n");

        if (g_ddraw.primary && g_ddraw.primary->palette)
        {
            g_ddraw.primary->selected_pal_count = GetPaletteEntries(hPal, 0, 256, g_ddraw.primary->selected_pal);

            return real_SelectPalette(g_ddraw.primary->hdc, hPal, bForceBkgd);;
        }
    }

    return real_SelectPalette(hdc, hPal, bForceBkgd);
}

UINT WINAPI fake_RealizePalette(HDC hdc)
{
    TRACE_EXT("%s(hdc=%p) [%p]\n", __FUNCTION__, hdc, _ReturnAddress());

    if (g_ddraw.ref &&
        g_ddraw.bpp == 8 &&
        ((g_ddraw.hwnd && WindowFromDC(hdc) == g_ddraw.hwnd) || WindowFromDC(hdc) == GetDesktopWindow()))
    {
        TRACE_EXT("     Display DC\n");

        if (g_ddraw.primary && g_ddraw.primary->palette)
        {
            if (g_ddraw.primary->selected_pal_count != 256)
            {
                TRACE_EXT("     selected_pal_count = %u\n", g_ddraw.primary->selected_pal_count);
            }

            ddp_SetEntries(
                g_ddraw.primary->palette, 
                DDPCAPS_REFRESH_CHANGED_ONLY,
                0, 
                g_ddraw.primary->selected_pal_count,
                g_ddraw.primary->selected_pal);

            return g_ddraw.primary->selected_pal_count;
        }
    }

    return real_RealizePalette(hdc);
}

HMODULE WINAPI fake_LoadLibraryA(LPCSTR lpLibFileName)
{
    HMODULE hmod_old = GetModuleHandleA(lpLibFileName);
    HMODULE hmod = real_LoadLibraryA(lpLibFileName);

#ifdef _DEBUG
    char mod_path[MAX_PATH] = { 0 };
    if (hmod && hmod != hmod_old && GetModuleFileNameA(hmod, mod_path, MAX_PATH))
    {
        TRACE("LoadLibraryA Module %s = %p (%s) [%p]\n", mod_path, hmod, lpLibFileName, _ReturnAddress());
    }
#endif

    if (hmod && hmod != g_ddraw_module && lpLibFileName &&
        (_strcmpi(lpLibFileName, "dinput.dll") == 0 || _strcmpi(lpLibFileName, "dinput") == 0 ||
            _strcmpi(lpLibFileName, "dinput8.dll") == 0 || _strcmpi(lpLibFileName, "dinput8") == 0))
    {
        dinput_hook_init();
    }

    if (hmod && hmod != hmod_old)
    {
        hook_init();
    }

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryW(LPCWSTR lpLibFileName)
{
    HMODULE hmod_old = GetModuleHandleW(lpLibFileName);
    HMODULE hmod = real_LoadLibraryW(lpLibFileName);

#ifdef _DEBUG
    char mod_path[MAX_PATH] = { 0 };
    if (hmod && hmod != hmod_old && GetModuleFileNameA(hmod, mod_path, MAX_PATH))
    {
        TRACE("LoadLibraryW Module %s = %p [%p]\n", mod_path, hmod, _ReturnAddress());
    }
#endif

    if (hmod && hmod != g_ddraw_module && lpLibFileName &&
        (_wcsicmp(lpLibFileName, L"dinput.dll") == 0 || _wcsicmp(lpLibFileName, L"dinput") == 0 ||
            _wcsicmp(lpLibFileName, L"dinput8.dll") == 0 || _wcsicmp(lpLibFileName, L"dinput8") == 0))
    {
        dinput_hook_init();
    }

    if (hmod && hmod != hmod_old)
    {
        hook_init();
    }

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod_old = GetModuleHandleA(lpLibFileName);
    HMODULE hmod = real_LoadLibraryExA(lpLibFileName, hFile, dwFlags);

#ifdef _DEBUG
    char mod_path[MAX_PATH] = { 0 };
    if (hmod && hmod != hmod_old && GetModuleFileNameA(hmod, mod_path, MAX_PATH))
    {
        TRACE("LoadLibraryExA Module %s = %p (%s) [%p]\n", mod_path, hmod, lpLibFileName, _ReturnAddress());
    }
#endif

    if (hmod && hmod != g_ddraw_module && lpLibFileName &&
        (_strcmpi(lpLibFileName, "dinput.dll") == 0 || _strcmpi(lpLibFileName, "dinput") == 0 ||
            _strcmpi(lpLibFileName, "dinput8.dll") == 0 || _strcmpi(lpLibFileName, "dinput8") == 0))
    {
        dinput_hook_init();
    }

    if (hmod && hmod != hmod_old)
    {
        hook_init();
    }

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod_old = GetModuleHandleW(lpLibFileName);
    HMODULE hmod = real_LoadLibraryExW(lpLibFileName, hFile, dwFlags);

#ifdef _DEBUG
    char mod_path[MAX_PATH] = { 0 };
    if (hmod && hmod != hmod_old && GetModuleFileNameA(hmod, mod_path, MAX_PATH))
    {
        TRACE("LoadLibraryExW Module %s = %p [%p]\n", mod_path, hmod, _ReturnAddress());
    }
#endif

    if (hmod && hmod != g_ddraw_module && lpLibFileName &&
        (_wcsicmp(lpLibFileName, L"dinput.dll") == 0 || _wcsicmp(lpLibFileName, L"dinput") == 0 ||
            _wcsicmp(lpLibFileName, L"dinput8.dll") == 0 || _wcsicmp(lpLibFileName, L"dinput8") == 0))
    {
        dinput_hook_init();
    }

    if (hmod && hmod != hmod_old)
    {
        hook_init();
    }

    return hmod;
}

FARPROC WINAPI fake_GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
#ifdef _DEBUG
    char mod_path[MAX_PATH] = { 0 };
    if (hModule && GetModuleFileNameA(hModule, mod_path, MAX_PATH))
    {
        TRACE("GetProcAddress %s (%s) [%p]\n", HIWORD(lpProcName) ? lpProcName : NULL, mod_path, _ReturnAddress());
    }
#endif

    BOOL hook = g_config.hook == 3;

#if defined(__GNUC__)
    if (g_config.hook == 4 && hModule && HIWORD(lpProcName))
    {
        if (strcmp(lpProcName, "DirectInputCreateA") == 0 ||
            strcmp(lpProcName, "DirectInputCreateEx") == 0 ||
            strcmp(lpProcName, "DirectInput8Create") == 0)
        {
            hook = TRUE;
            g_dinput_hook_active = TRUE;
        }
    }
#endif

    FARPROC proc = real_GetProcAddress(hModule, lpProcName);

    if (!hook || !hModule || !HIWORD(lpProcName))
        return proc;

    for (int i = 0; g_hook_hooklist[i].module_name[0]; i++)
    {
        HMODULE mod = GetModuleHandleA(g_hook_hooklist[i].module_name);

        if (hModule != mod)
            continue;

        for (int x = 0; g_hook_hooklist[i].data[x].function_name[0]; x++)
        {
            if (strcmp(lpProcName, g_hook_hooklist[i].data[x].function_name) == 0)
            {
                return (FARPROC)g_hook_hooklist[i].data[x].new_function;
            }
        }
    }

    return proc;
}

BOOL WINAPI fake_GetDiskFreeSpaceA(
    LPCSTR lpRootPathName,
    LPDWORD lpSectorsPerCluster,
    LPDWORD lpBytesPerSector,
    LPDWORD lpNumberOfFreeClusters,
    LPDWORD lpTotalNumberOfClusters)
{
    BOOL result = 
        real_GetDiskFreeSpaceA(
            lpRootPathName,
            lpSectorsPerCluster,
            lpBytesPerSector,
            lpNumberOfFreeClusters,
            lpTotalNumberOfClusters);

    if (result && lpSectorsPerCluster && lpBytesPerSector && lpNumberOfFreeClusters)
    {
        long long int free_bytes = (long long int)*lpNumberOfFreeClusters * *lpSectorsPerCluster * *lpBytesPerSector;

        if (free_bytes >= 2147155968)
        {
            *lpSectorsPerCluster = 0x00000040;
            *lpBytesPerSector = 0x00000200;
            *lpNumberOfFreeClusters = 0x0000FFF6;

            if (lpTotalNumberOfClusters)
                *lpTotalNumberOfClusters = 0x0000FFF6;
        }
    }

    return result; 
}

DWORD WINAPI fake_GetVersion()
{
    if (_strcmpi(g_config.win_version, "95") == 0) 
        return 0xC3B60004;

    if (_strcmpi(g_config.win_version, "98") == 0)
        return 0xC0000A04;

    if (_strcmpi(g_config.win_version, "nt4") == 0)
        return 0x05650004;

    if (_strcmpi(g_config.win_version, "2000") == 0)
        return 0x08930005;

    if (_strcmpi(g_config.win_version, "xp") == 0)
        return 0x0A280105;

    return real_GetVersion();
}

BOOL WINAPI fake_GetVersionExA(LPOSVERSIONINFOA info)
{
    if (g_config.win_version[0] && info && info->dwOSVersionInfoSize == sizeof(OSVERSIONINFOA))
    {
        if (_strcmpi(g_config.win_version, "95") == 0)
        {
            *info = (OSVERSIONINFOA){ sizeof(OSVERSIONINFOA), 4, 0, 950, 1, "" };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "98") == 0)
        {
            *info = (OSVERSIONINFOA){ sizeof(OSVERSIONINFOA), 4, 10, 67766446, 1, "" };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "nt4") == 0) 
        {
            *info = (OSVERSIONINFOA){ sizeof(OSVERSIONINFOA), 4, 0, 1381, 2, "Service Pack 5" };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "2000") == 0) 
        {
            *info = (OSVERSIONINFOA){ sizeof(OSVERSIONINFOA), 5, 0, 2195, 2, "" };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "xp") == 0) 
        {
            *info = (OSVERSIONINFOA){ sizeof(OSVERSIONINFOA), 5, 1, 2600, 2, "Service Pack 3" };
            return TRUE;
        }
    }

    if (g_config.win_version[0] && info && info->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA))
    {
        LPOSVERSIONINFOEXA ex = (LPOSVERSIONINFOEXA)info;
        if (_strcmpi(g_config.win_version, "95") == 0)
        {
            *ex = (OSVERSIONINFOEXA){ sizeof(OSVERSIONINFOEXA), 4, 0, 950, 1, "", 1, 0, 256, 1, 30 };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "98") == 0) 
        {
            *ex = (OSVERSIONINFOEXA){ sizeof(OSVERSIONINFOEXA), 4, 10, 67766446, 1, "", 1, 0, 256, 1, 30 };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "nt4") == 0) 
        {
            *ex = (OSVERSIONINFOEXA){ sizeof(OSVERSIONINFOEXA), 4, 0, 1381, 2, "Service Pack 5", 5, 0, 256, 1, 30 };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "2000") == 0) 
        {
            *ex = (OSVERSIONINFOEXA){ sizeof(OSVERSIONINFOEXA), 5, 0, 2195, 2, "", 0, 0, 256, 1, 30 };
            return TRUE;
        }

        if (_strcmpi(g_config.win_version, "xp") == 0) 
        {
            *ex = (OSVERSIONINFOEXA){ sizeof(OSVERSIONINFOEXA), 5, 1, 2600, 2, "Service Pack 3", 3, 0, 256, 1, 30 };
            return TRUE;
        }
    }

    return real_GetVersionExA(info);
}

BOOL WINAPI fake_DestroyWindow(HWND hWnd)
{
    TRACE("DestroyWindow(hwnd=%p) - g_ddraw.hwnd=%p [%p]\n", hWnd, g_ddraw.hwnd, _ReturnAddress());

    if (g_ddraw.ref && hWnd && hWnd == g_ddraw.hwnd)
    {
        dd_RestoreDisplayMode();

        if (g_ddraw.renderer == d3d9_render_main)
        {
            d3d9_release();
        }
        else if (g_ddraw.renderer == ogl_render_main)
        {
            ogl_release();
        }
    }

    BOOL result = real_DestroyWindow(hWnd);

    if (result && g_ddraw.ref && hWnd && hWnd == g_ddraw.hwnd)
    {
        g_ddraw.hwnd = NULL;
        g_ddraw.wndproc = NULL;
        g_ddraw.render.hdc = NULL;

        if (g_config.fake_mode[0])
        {
            dd_SetCooperativeLevel(NULL, DDSCL_NORMAL);
        }
    }

    if (g_ddraw.ref && g_ddraw.hwnd != hWnd && g_ddraw.bnet_active)
    {
        RedrawWindow(NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

        if (!FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL))
        {
            g_ddraw.bnet_active = FALSE;
            SetFocus(g_ddraw.hwnd);
            mouse_lock();

            if (g_config.windowed)
            {
                g_ddraw.bnet_pos.x = g_ddraw.bnet_pos.y = 0;
                real_ClientToScreen(g_ddraw.hwnd, &g_ddraw.bnet_pos);

                if (!g_ddraw.bnet_was_upscaled)
                {
                    int width = g_ddraw.bnet_win_rect.right - g_ddraw.bnet_win_rect.left;
                    int height = g_ddraw.bnet_win_rect.bottom - g_ddraw.bnet_win_rect.top;

                    UINT flags = width != g_ddraw.width || height != g_ddraw.height ? 0 : SWP_NOMOVE;

                    int dst_width = width == g_ddraw.width ? 0 : width;
                    int dst_height = height == g_ddraw.height ? 0 : height;

                    util_set_window_rect(
                        g_ddraw.bnet_win_rect.left, 
                        g_ddraw.bnet_win_rect.top, 
                        dst_width, 
                        dst_height, 
                        flags);
                }

                g_config.fullscreen = g_ddraw.bnet_was_upscaled;

                SetTimer(g_ddraw.hwnd, IDT_TIMER_LEAVE_BNET, 1000, (TIMERPROC)NULL);

                g_config.resizable = TRUE;
            }
        }
    }

    return result;
}

HWND WINAPI fake_CreateWindowExA(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
    int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    TRACE("-> CreateWindowExA("
        "dwExStyle=%08X, lpClassName=%p, lpWindowName=%p, dwStyle=%08X, X=%d, Y=%d, nWidth=%d, "
        "nHeight=%d, hWndParent=%p, hMenu=%p, hInstance=%p, lpParam=%p) [%p]\n", 
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
        lpParam, 
        _ReturnAddress());

    TRACE("     WindowName=%s, ClassName=%s, g_ddraw.hwnd=%p\n", lpWindowName, HIWORD(lpClassName) ? lpClassName : "", g_ddraw.hwnd);

    dbg_dump_wnd_styles(dwStyle, dwExStyle);

    /* Almost all of the Learning Company Games */
    if (!dwExStyle &&
        HIWORD(lpClassName) && _strcmpi(lpClassName, "OMWindowChildClass") == 0 &&
        !lpWindowName &&
        dwStyle == (WS_CHILD | WS_CHILDWINDOW | WS_CLIPSIBLINGS) &&
        !X &&
        !Y &&
        g_ddraw.ref && g_ddraw.width && g_ddraw.width == nWidth && g_ddraw.height == nHeight &&
        g_ddraw.hwnd && hWndParent == g_ddraw.hwnd &&
        !hMenu &&
        !g_config.game_section[0])
    {
        dwExStyle = WS_EX_TRANSPARENT;
        g_config.lock_mouse_top_left = TRUE;
        g_config.adjmouse = FALSE;
        dd_SetDisplayMode(0, 0, 0, 0);
    }

    /* The American Girls Dress Designer */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "AfxFrameOrView42s") == 0 &&
        g_ddraw.ref && g_ddraw.hwnd && hWndParent == g_ddraw.hwnd &&
        g_config.fake_mode[0] &&
        (dwStyle & (WS_POPUP | WS_CHILD)) == (WS_POPUP | WS_CHILD))
    {
        dwStyle &= ~WS_POPUP;
    }

    /* Dark Colony */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "Merc Direct Draw Driver") == 0 &&
        lpWindowName && _strcmpi(lpWindowName, "Direct Draw Driver") == 0 &&
        !dwExStyle)
    {
        dwExStyle |= WS_EX_APPWINDOW;
    }

    /* Fallout 1/2 */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "GNW95 Class") == 0 &&
        lpWindowName && strstr(lpWindowName, "FALLOUT"))
    {
        /* Workaround for window not showing up in taskbar sometimes */
        dwExStyle |= WS_EX_APPWINDOW;
    }

    /* Fallout Tactics */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "bosWin32Class") == 0 &&
        lpWindowName && _strcmpi(lpWindowName, "Fallout: Tactics (TM)") == 0)
    {
        dwStyle |= WS_VISIBLE;
    }

    /* Center Claw DVD movies */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "Afx:400000:3") == 0 &&
        g_ddraw.ref && g_ddraw.hwnd && hWndParent == g_ddraw.hwnd &&
        g_ddraw.width &&
        (dwStyle & (WS_POPUP | WS_CHILD)) == (WS_POPUP | WS_CHILD))
    {
        //dwStyle &= ~WS_POPUP;
        //dwExStyle = WS_EX_TRANSPARENT;

        POINT pt = { 0, 0 };
        real_ClientToScreen(g_ddraw.hwnd, &pt);

        int added_height = g_ddraw.render.height - g_ddraw.height;
        int added_width = g_ddraw.render.width - g_ddraw.width;
        int align_y = added_height > 0 ? added_height / 2 : 0;
        int align_x = added_width > 0 ? added_width / 2 : 0;

        X = pt.x + align_x;
        Y = pt.y + align_y;
    }

    /* Metal Knight Movies */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "Afx:400000:3:0:1900011:0") == 0 &&
        lpWindowName && _strcmpi(lpWindowName, "AVI player") == 0 &&
        dwStyle == WS_POPUP &&
        dwExStyle == WS_EX_TOPMOST &&
        g_ddraw.ref && g_ddraw.hwnd &&
        g_ddraw.width)
    {
        dwExStyle = 0;

        POINT pt = { 0, 0 };
        real_ClientToScreen(g_ddraw.hwnd, &pt);

        int added_height = g_ddraw.render.height - g_ddraw.height;
        int added_width = g_ddraw.render.width - g_ddraw.width;
        int align_y = added_height > 0 ? added_height / 2 : 0;
        int align_x = added_width > 0 ? added_width / 2 : 0;

        X += pt.x + align_x;
        Y += pt.y + align_y;
    }

    /* Disney Trivia Challenge */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "Disney Trivia Challenge") == 0 &&
        hWndParent && (dwStyle & WS_CHILD))
    {
        dwExStyle = WS_EX_TRANSPARENT;
    }

    /* Center Lego Loco overlays */
    if (lpWindowName && _strcmpi(lpWindowName, "LEGO LOCO") == 0 &&
        g_ddraw.ref && g_ddraw.hwnd && hWndParent == g_ddraw.hwnd &&
        g_ddraw.width &&
        (dwStyle & WS_POPUP))
    {
        /* not working currently, game probably moves it with SetWindowPos or MoveWindow afterwards
        POINT pt = { 0, 0 };
        real_ClientToScreen(g_ddraw.hwnd, &pt);

        int added_height = g_ddraw.render.height - g_ddraw.height;
        int added_width = g_ddraw.render.width - g_ddraw.width;
        int align_y = added_height > 0 ? added_height / 2 : 0;
        int align_x = added_width > 0 ? added_width / 2 : 0;

        X = pt.x + align_x;
        Y = pt.y + align_y;
        */
    }

    /* Road Rash movies */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "AVI Window") == 0 &&
        g_ddraw.ref && g_ddraw.hwnd && g_ddraw.width &&
        (dwStyle & WS_POPUP))
    {
        dwStyle = WS_CHILD;
        hWndParent = g_ddraw.hwnd;
    }

    /* A Bug's Life Action Game */
    //HIWORD(lpClassName) && _strcmpi(lpClassName, "Bugs") == 0 &&
    if (dwExStyle == 0x01000008)
    {
        dwExStyle = WS_EX_TOPMOST;
    }

    /* Fix for SMACKW32.DLL creating another window that steals the focus */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "MouseTypeWind") == 0 && g_ddraw.ref && g_ddraw.hwnd)
    {
        dwStyle &= ~WS_VISIBLE;
    }

    /* Battle.net */
    if (HIWORD(lpClassName) && _strcmpi(lpClassName, "SDlgDialog") == 0 && g_ddraw.ref && g_ddraw.hwnd)
    {
        if (!g_ddraw.bnet_active)
        {
            g_ddraw.bnet_was_upscaled = g_config.fullscreen;
            g_config.fullscreen = FALSE;

            if (!g_config.windowed && !g_ddraw.bnet_was_fullscreen)
            {
                int ws = g_config.window_state;
                util_toggle_fullscreen();
                g_config.window_state = ws;
                g_ddraw.bnet_was_fullscreen = TRUE;
            }

            real_GetClientRect(g_ddraw.hwnd, &g_ddraw.bnet_win_rect);
            real_MapWindowPoints(g_ddraw.hwnd, HWND_DESKTOP, (LPPOINT)&g_ddraw.bnet_win_rect, 2);

            int width = g_ddraw.bnet_win_rect.right - g_ddraw.bnet_win_rect.left;
            int height = g_ddraw.bnet_win_rect.bottom - g_ddraw.bnet_win_rect.top;

            int x = g_ddraw.bnet_pos.x || g_ddraw.bnet_pos.y ? g_ddraw.bnet_pos.x : -32000;
            int y = g_ddraw.bnet_pos.x || g_ddraw.bnet_pos.y ? g_ddraw.bnet_pos.y : -32000;

            UINT flags = width != g_ddraw.width || height != g_ddraw.height ? 0 : SWP_NOMOVE;

            int dst_width = g_config.window_rect.right ? g_ddraw.width : 0;
            int dst_height = g_config.window_rect.bottom ? g_ddraw.height : 0;

            util_set_window_rect(x, y, dst_width, dst_height, flags);
            g_config.resizable = FALSE;

            g_ddraw.bnet_active = TRUE;
            mouse_unlock();
            ReleaseCapture();
        }

        POINT pt = { 0, 0 };
        real_ClientToScreen(g_ddraw.hwnd, &pt);

        int added_height = g_ddraw.height - 480;
        int added_width = g_ddraw.width - 640;
        int align_y = added_height > 0 ? added_height / 2 : 0;
        int align_x = added_width > 0 ? added_width / 2 : 0;

        X += pt.x + align_x;
        Y += pt.y + align_y;

        dwStyle |= WS_CLIPCHILDREN;
    }

    /* Limit window size to max surface size (Dune2000 1.02) */
    if (dwStyle & WS_POPUP)
    {
        if (nWidth != CW_USEDEFAULT)
        {
            nWidth = min(nWidth, 16384);
        }
        
        if (nHeight != CW_USEDEFAULT)
        {
            nHeight = min(nHeight, 16384);
        }
    }

    HWND hwnd = real_CreateWindowExA(
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

    /* Age Of Empires 2 textbox align */
    if (!dwExStyle &&
        HIWORD(lpClassName) && _strcmpi(lpClassName, "edit") == 0 &&
        !lpWindowName &&
        g_ddraw.ref && g_ddraw.width &&
        g_ddraw.hwnd && hWndParent == g_ddraw.hwnd &&
        (int)hMenu == 1)
    {
        g_ddraw.textbox.hwnd = hwnd;
    }

    TRACE("<- CreateWindowExA(hwnd=%p)\n", hwnd);

    return hwnd;
}

HRESULT WINAPI fake_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv)
{
    if (rclsid && riid)
    {
        TRACE(
            "CoCreateInstance rclsid = %08X, riid = %08X [%p]\n", 
            ((GUID*)rclsid)->Data1,
            ((GUID*)riid)->Data1, 
            _ReturnAddress());

        if (IsEqualGUID(&CLSID_DirectDraw, rclsid) || IsEqualGUID(&CLSID_DirectDraw7, rclsid))
        {
            TRACE("     GUID = %08X (CLSID_DirectDrawX)\n", ((GUID*)rclsid)->Data1);

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

        if (IsEqualGUID(&CLSID_DirectDrawClipper, rclsid))
        {
            TRACE("     GUID = %08X (CLSID_DirectDrawClipper)\n", ((GUID*)rclsid)->Data1);

            if (IsEqualGUID(&IID_IDirectDrawClipper, riid))
            {
                return dd_CreateClipper(0, (IDirectDrawClipperImpl**)ppv, NULL);
            }
        }
    }

    /* These dlls must be hooked for cutscene uscaling and windowed mode */
    HMODULE quartz_dll = GetModuleHandleA("quartz");
    HMODULE msvfw32_dll = GetModuleHandleA("msvfw32");

    HRESULT result = real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    if ((!quartz_dll && GetModuleHandleA("quartz")) ||
        (!msvfw32_dll && GetModuleHandleA("msvfw32")))
    {
        hook_init();
    }

    return result;
}

MCIERROR WINAPI fake_mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    /* These dlls must be hooked for cutscene uscaling and windowed mode */
    HMODULE quartz_dll = GetModuleHandleA("quartz");
    HMODULE msvfw32_dll = GetModuleHandleA("msvfw32");

    MCIERROR result = real_mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);

    if ((!quartz_dll && GetModuleHandleA("quartz")) ||
        (!msvfw32_dll && GetModuleHandleA("msvfw32")))
    {
        hook_init();
    }

    return result;
}

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI fake_SetUnhandledExceptionFilter(
    LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
    LPTOP_LEVEL_EXCEPTION_FILTER old = g_dbg_exception_filter;
    g_dbg_exception_filter = lpTopLevelExceptionFilter;

    return old;
    //return real_SetUnhandledExceptionFilter(lpTopLevelExceptionFilter);
}

PGETFRAME WINAPI fake_AVIStreamGetFrameOpen(PAVISTREAM pavi, LPBITMAPINFOHEADER lpbiWanted)
{
    if (g_ddraw.ref && g_ddraw.primary && (DWORD)lpbiWanted == AVIGETFRAMEF_BESTDISPLAYFMT)
    {
        DDBITMAPINFO bmi;
        memcpy(&bmi, g_ddraw.primary->bmi, sizeof(DDBITMAPINFO));

        bmi.bmiHeader.biHeight = 0;
        bmi.bmiHeader.biWidth = 0;

        if (g_ddraw.bpp == 8 && g_ddraw.primary->palette)
        {
            memcpy(&bmi.bmiColors[0], g_ddraw.primary->palette->data_rgb, sizeof(bmi.bmiColors));
        }

        PGETFRAME result = real_AVIStreamGetFrameOpen(pavi, (LPBITMAPINFOHEADER)&bmi);

        if (result)
            return result;
    }

    return real_AVIStreamGetFrameOpen(pavi, lpbiWanted);
}
