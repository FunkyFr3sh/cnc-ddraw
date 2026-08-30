#include <windows.h>
#include <windowsx.h>
#include <math.h>
#include "dllmain.h"
#include "dd.h"
#include "hook.h"
#include "mouse.h"
#include "render_d3d9.h"
#include "config.h"
#include "screenshot.h"
#include "winapi_hooks.h"
#include "directinput.h"
#include "wndproc.h"
#include "utils.h"
#include "debug.h"
#include "versionhelpers.h"


LRESULT CALLBACK fake_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
    if (uMsg != WM_MOUSEMOVE && uMsg != WM_NCMOUSEMOVE && uMsg != WM_NCHITTEST && uMsg != WM_SETCURSOR &&
        uMsg != WM_KEYUP && uMsg != WM_KEYDOWN && uMsg != WM_CHAR && uMsg != WM_DEADCHAR && uMsg != WM_INPUT &&
        uMsg != WM_UNICHAR && uMsg != WM_IME_CHAR && uMsg != WM_IME_KEYDOWN && uMsg != WM_IME_KEYUP && uMsg != WM_TIMER &&
        uMsg != WM_D3D9DEVICELOST && uMsg != WM_NULL)
    {
        TRACE(
            "     uMsg = %s (%d), wParam = %08X (%d), lParam = %08X (%d, LO=%d HI=%d)\n",
            dbg_mes_to_str(uMsg),
            uMsg,
            wParam,
            wParam,
            lParam,
            lParam,
            (int)(short)LOWORD(lParam),
            (int)(short)HIWORD(lParam));
    }
#endif

    static BOOL in_size_move = FALSE;
    static int redraw_count = 0;

    switch (uMsg)
    {
    case WM_NULL:
    case WM_MOVING:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCPAINT:
    case WM_CANCELMODE:
    case WM_DISPLAYCHANGE:
    case WM_NCCALCSIZE:
    {
        return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;

        if (g_config.windowed && g_ddraw.width)
        {
            RECT rc = { 0, 0, g_ddraw.render.width, g_ddraw.render.height };

            AdjustWindowRectEx(
                &rc,
                real_GetWindowLongA(g_ddraw.hwnd, GWL_STYLE),
                GetMenu(g_ddraw.hwnd) != NULL,
                real_GetWindowLongA(g_ddraw.hwnd, GWL_EXSTYLE));

            if (mmi->ptMaxTrackSize.x < rc.right - rc.left)
                mmi->ptMaxTrackSize.x = rc.right - rc.left;

            if (mmi->ptMaxTrackSize.y < rc.bottom - rc.top)
                mmi->ptMaxTrackSize.y = rc.bottom - rc.top;

            /*
            RECT rcmin = { 0, 0, g_ddraw.width, g_ddraw.height };

            AdjustWindowRectEx(
                &rcmin,
                real_GetWindowLongA(g_ddraw.hwnd, GWL_STYLE),
                GetMenu(g_ddraw.hwnd) != NULL,
                real_GetWindowLongA(g_ddraw.hwnd, GWL_EXSTYLE));

            mmi->ptMinTrackSize.x = rcmin.right - rcmin.left;
            mmi->ptMinTrackSize.y = rcmin.bottom - rcmin.top;
            */

            return 0;
        }

        return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }
    case WM_KILLFOCUS:
    case WM_NCACTIVATE:
    {
        if (g_config.noactivateapp)
        {
            return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }

        break;
    }
    case WM_NCHITTEST:
    {
        if (g_mouse_locked || g_config.devmode)
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            if (!g_config.windowed || real_ScreenToClient(g_ddraw.hwnd, &pt))
            {
                int x = max(pt.x - g_ddraw.mouse.x_adjust, 0);
                int y = max(pt.y - g_ddraw.mouse.y_adjust, 0);

                if (g_config.adjmouse)
                {
                    x = (DWORD)(roundf(x * g_ddraw.mouse.unscale_x));
                    y = (DWORD)(roundf(y * g_ddraw.mouse.unscale_y));
                }

                pt.x = min(x, g_ddraw.width - 1);
                pt.y = min(y, g_ddraw.height - 1);
            }
            else
            {
                pt.x = InterlockedExchangeAdd((LONG*)&g_ddraw.cursor.x, 0);
                pt.y = InterlockedExchangeAdd((LONG*)&g_ddraw.cursor.y, 0);
            }

            CallWindowProcA(g_ddraw.wndproc, hWnd, uMsg, wParam, MAKELPARAM(pt.x, pt.y));
        }

        LRESULT result = real_DefWindowProcA(hWnd, uMsg, wParam, lParam);

        if (!g_config.resizable)
        {
            switch (result)
            {
            case HTBOTTOM:
            case HTBOTTOMLEFT:
            case HTBOTTOMRIGHT:
            case HTLEFT:
            case HTRIGHT:
            case HTTOP:
            case HTTOPLEFT:
            case HTTOPRIGHT:
                return HTBORDER;
            }
        }

        return result;
    }
    case WM_SETCURSOR:
    {
        /* show resize cursor on window borders */
        if ((HWND)wParam == g_ddraw.hwnd)
        {
            WORD message = HIWORD(lParam);

            if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN)
            {
                WORD htcode = LOWORD(lParam);

                switch (htcode)
                {
                case HTCAPTION:
                case HTMINBUTTON:
                case HTMAXBUTTON:
                case HTCLOSE:
                case HTBOTTOM:
                case HTBOTTOMLEFT:
                case HTBOTTOMRIGHT:
                case HTLEFT:
                case HTRIGHT:
                case HTTOP:
                case HTTOPLEFT:
                case HTTOPRIGHT:
                    return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);
                case HTCLIENT:
                    if (!g_mouse_locked && !g_config.devmode)
                    {
                        real_SetCursor(LoadCursor(NULL, IDC_ARROW));
                        return TRUE;
                    }
                default:
                    break;
                }
            }
        }

        break;
    }
    case WM_SIZE_DDRAW:
    {
        uMsg = WM_SIZE;
        break;
    }
    case WM_MOVE_DDRAW:
    {
        uMsg = WM_MOVE;
        break;
    }
    case WM_DISPLAYCHANGE_DDRAW:
    {
        uMsg = WM_DISPLAYCHANGE;
        break;
    }
    case WM_D3D9DEVICELOST:
    {
        if (((!g_config.windowed && !g_config.nonexclusive) || !util_is_minimized(g_ddraw.hwnd)) &&
            g_ddraw.renderer == d3d9_render_main &&
            d3d9_on_device_lost())
        {
            if (!g_config.windowed)
                mouse_lock();
        }
        return 0;
    }
    case WM_TIMER:
    {
        switch (wParam)
        {
        case IDT_TIMER_LEAVE_BNET:
        {
            KillTimer(g_ddraw.hwnd, IDT_TIMER_LEAVE_BNET);

            if (!g_config.windowed)
                g_ddraw.bnet_was_fullscreen = FALSE;

            if (!g_ddraw.bnet_active)
            {
                if (g_ddraw.bnet_was_fullscreen)
                {
                    int ws = g_config.window_state;
                    util_toggle_fullscreen();
                    g_config.window_state = ws;
                    g_ddraw.bnet_was_fullscreen = FALSE;
                }
                else if (g_ddraw.bnet_was_upscaled)
                {
                    util_set_window_rect(0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    g_ddraw.bnet_was_upscaled = FALSE;
                }
            }

            return 0;
        }
        case IDT_TIMER_LINUX_FIX_WINDOW_SIZE:
        {
            KillTimer(g_ddraw.hwnd, IDT_TIMER_LINUX_FIX_WINDOW_SIZE);
            util_set_window_rect(0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            return 0;
        }
        }
        break;
    }
    case WM_WINDOWPOSCHANGED:
    {
        WINDOWPOS* pos = (WINDOWPOS*)lParam;

        /*
        dbg_dump_swp_flags(pos->flags);
        TRACE(
            "     hwndInsertAfter=%p, x=%d, y=%d, cx=%d, cy=%d\n",
            pos->hwndInsertAfter, pos->x, pos->y, pos->cx, pos->cy);
        */

        if (IsWine() &&
            !g_config.windowed &&
            (pos->x > 0 || pos->y > 0) &&
            g_ddraw.last_set_window_pos_tick + 500 < timeGetTime())
        {
            PostMessage(g_ddraw.hwnd, WM_WINEFULLSCREEN, 0, 0);
        }

        /* cnc-ddraw-macfix: window moved -> re-clip cursor to new position */
        if (g_config.windowed && g_mouse_locked)
            mouse_update_clip();

        break;
    }
    case WM_WINEFULLSCREEN:
    {
        if (!g_config.windowed)
        {
            g_ddraw.last_set_window_pos_tick = timeGetTime();

            int menu_height = GetMenu(g_ddraw.hwnd) ? real_GetSystemMetrics(SM_CYMENU) : 0;

            real_SetWindowPos(
                g_ddraw.hwnd,
                HWND_TOPMOST,
                1,
                1,
                g_ddraw.render.width,
                g_ddraw.render.height + menu_height,
                SWP_SHOWWINDOW);

            real_SetWindowPos(
                g_ddraw.hwnd,
                HWND_TOPMOST,
                0,
                0,
                g_ddraw.render.width,
                g_ddraw.render.height + menu_height,
                SWP_SHOWWINDOW);
        }
        return 0;
    }
    case WM_ENTERSIZEMOVE:
    {
        if (g_config.windowed)
        {
            in_size_move = TRUE;
        }
        break;
    }
    case WM_EXITSIZEMOVE:
    {
        if (g_config.windowed)
        {
            in_size_move = FALSE;

            if (!g_ddraw.render.thread)
                dd_SetDisplayMode(0, 0, 0, 0);
        }
        break;
    }
    case WM_SIZING:
    {
        RECT* windowrc = (RECT*)lParam;

        if (g_config.windowed)
        {
            if (in_size_move)
            {
                if (g_ddraw.render.thread)
                {
                    EnterCriticalSection(&g_ddraw.cs);
                    g_ddraw.render.run = FALSE;
                    ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
                    LeaveCriticalSection(&g_ddraw.cs);

                    WaitForSingleObject(g_ddraw.render.thread, INFINITE);
                    g_ddraw.render.thread = NULL;
                }

                RECT clientrc = { 0 };

                /* maintain aspect ratio */
                if (g_config.maintas &&
                    CopyRect(&clientrc, windowrc) &&
                    util_unadjust_window_rect(
                        &clientrc, 
                        real_GetWindowLongA(hWnd, GWL_STYLE),
                        GetMenu(hWnd) != NULL,
                        real_GetWindowLongA(hWnd, GWL_EXSTYLE)) &&
                    SetRect(&clientrc, 0, 0, clientrc.right - clientrc.left, clientrc.bottom - clientrc.top))
                {
                    double scale_h;
                    double scale_w;

                    if (g_config.aspect_ratio[0])
                    {
                        char* e = &g_config.aspect_ratio[0];

                        DWORD cx = strtoul(e, &e, 0);
                        DWORD cy = strtoul(e + 1, &e, 0);

                        scale_h = (double)cy / cx;
                        scale_w = (double)cx / cy;
                    }
                    else
                    {
                        scale_h = (double)g_ddraw.height / g_ddraw.width;
                        scale_w = (double)g_ddraw.width / g_ddraw.height;
                    }

                    switch (wParam)
                    {
                    case WMSZ_BOTTOMLEFT:
                    case WMSZ_BOTTOMRIGHT:
                    case WMSZ_LEFT:
                    case WMSZ_RIGHT:
                    {
                        windowrc->bottom += (LONG)round(scale_h * clientrc.right - clientrc.bottom);
                        break;
                    }
                    case WMSZ_TOP:
                    case WMSZ_BOTTOM:
                    {
                        windowrc->right += (LONG)round(scale_w * clientrc.bottom - clientrc.right);
                        break;
                    }
                    case WMSZ_TOPRIGHT:
                    case WMSZ_TOPLEFT:
                    {
                        windowrc->top -= (LONG)round(scale_h * clientrc.right - clientrc.bottom);
                        break;
                    }
                    }
                }

                /* enforce minimum window size */
                if (CopyRect(&clientrc, windowrc) &&
                    util_unadjust_window_rect(
                        &clientrc, 
                        real_GetWindowLongA(hWnd, GWL_STYLE),
                        GetMenu(hWnd) != NULL,
                        real_GetWindowLongA(hWnd, GWL_EXSTYLE)) &&
                    SetRect(&clientrc, 0, 0, clientrc.right - clientrc.left, clientrc.bottom - clientrc.top))
                {
                    if (clientrc.right < g_ddraw.width)
                    {
                        switch (wParam)
                        {
                        case WMSZ_TOPRIGHT:
                        case WMSZ_BOTTOMRIGHT:
                        case WMSZ_RIGHT:
                        case WMSZ_BOTTOM:
                        case WMSZ_TOP:
                        {
                            windowrc->right += g_ddraw.width - clientrc.right;
                            break;
                        }
                        case WMSZ_TOPLEFT:
                        case WMSZ_BOTTOMLEFT:
                        case WMSZ_LEFT:
                        {
                            windowrc->left -= g_ddraw.width - clientrc.right;
                            break;
                        }
                        }
                    }

                    if (clientrc.bottom < g_ddraw.height)
                    {
                        switch (wParam)
                        {
                        case WMSZ_BOTTOMLEFT:
                        case WMSZ_BOTTOMRIGHT:
                        case WMSZ_BOTTOM:
                        case WMSZ_RIGHT:
                        case WMSZ_LEFT:
                        {
                            windowrc->bottom += g_ddraw.height - clientrc.bottom;
                            break;
                        }
                        case WMSZ_TOPLEFT:
                        case WMSZ_TOPRIGHT:
                        case WMSZ_TOP:
                        {
                            windowrc->top -= g_ddraw.height - clientrc.bottom;
                            break;
                        }
                        }
                    }
                }

                /* save new window position */
                if (CopyRect(&clientrc, windowrc) &&
                    util_unadjust_window_rect(
                        &clientrc, 
                        real_GetWindowLongA(hWnd, GWL_STYLE),
                        GetMenu(hWnd) != NULL,
                        real_GetWindowLongA(hWnd, GWL_EXSTYLE)))
                {
                    g_config.window_rect.left = clientrc.left;
                    g_config.window_rect.top = clientrc.top;
                    g_config.window_rect.right = clientrc.right - clientrc.left;
                    g_config.window_rect.bottom = clientrc.bottom - clientrc.top;
                }

                return TRUE;
            }
        }
        break;
    }
    case WM_SIZE:
    {
        if (g_config.windowed)
        {
            WORD width = LOWORD(lParam);
            WORD height = HIWORD(lParam);

            if (wParam == SIZE_RESTORED)
            {
                /* macOS maximize hack */
                if (in_size_move && g_ddraw.render.thread && IsMacOS())
                {
                    EnterCriticalSection(&g_ddraw.cs);
                    g_ddraw.render.run = FALSE;
                    ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
                    LeaveCriticalSection(&g_ddraw.cs);

                    WaitForSingleObject(g_ddraw.render.thread, INFINITE);
                    g_ddraw.render.thread = NULL;
                }

                if (in_size_move && !g_ddraw.render.thread)
                {
                    g_config.window_rect.right = width;
                    g_config.window_rect.bottom = height;
                }
                else if (!in_size_move && g_ddraw.render.thread && !g_config.fullscreen && IsLinux())
                {
                    if (width != g_ddraw.render.width || height != g_ddraw.render.height)
                    {
                        KillTimer(g_ddraw.hwnd, IDT_TIMER_LINUX_FIX_WINDOW_SIZE);

                        g_config.window_rect.right = width;
                        g_config.window_rect.bottom = height;

                        dd_SetDisplayMode(0, 0, 0, 0);

                        if (width < g_ddraw.width || height < g_ddraw.height)
                        {
                            /* Can't enforce minimum window size in linux because there is no WM_SIZING and 
                               WM_GETMINMAXINFO is ignored for whatever reasons */

                            SetTimer(g_ddraw.hwnd, IDT_TIMER_LINUX_FIX_WINDOW_SIZE, 1000, (TIMERPROC)NULL);
                        }
                    }
                }
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                if (!in_size_move && g_ddraw.render.thread && !g_config.fullscreen && IsLinux())
                {
                    if (width != g_ddraw.render.width || height != g_ddraw.render.height)
                    {
                        KillTimer(g_ddraw.hwnd, IDT_TIMER_LINUX_FIX_WINDOW_SIZE);

                        g_config.window_rect.right = width;
                        g_config.window_rect.bottom = height;

                        dd_SetDisplayMode(0, 0, 0, 0);

                        if (width < g_ddraw.width || height < g_ddraw.height)
                        {
                            /* Can't enforce minimum window size in linux because there is no WM_SIZING and
                               WM_GETMINMAXINFO is ignored for whatever reasons */

                            SetTimer(g_ddraw.hwnd, IDT_TIMER_LINUX_FIX_WINDOW_SIZE, 1000, (TIMERPROC)NULL);
                        }
                    }
                }
            }
        }

        if (g_ddraw.got_child_windows)
        {
            redraw_count = 2;
            RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }

        return real_DefWindowProcA(hWnd, uMsg, wParam, lParam); /* Carmageddon fix */
    }
    case WM_MOVE:
    {
        if (g_config.windowed)
        {
            int x = (int)(short)LOWORD(lParam);
            int y = (int)(short)HIWORD(lParam);

            if (x != -32000 && y != -32000)
            {
                util_update_bnet_pos(x, y);
            }

            /* cnc-ddraw-macfix: winemac never sends WM_ENTERSIZEMOVE for native
               title-bar drags, so in_size_move stays false and window_rect kept
               its -32000 (center) value -> cnc-ddraw re-centered the window back
               after every drag. Track the move on macOS too (like Linux). */
            if (in_size_move || ((IsLinux() || IsMacOS()) && !g_config.fullscreen && g_ddraw.render.thread))
            {
                if (x != -32000)
                    g_config.window_rect.left = x; /* -32000 = Exit/Minimize */

                if (y != -32000)
                    g_config.window_rect.top = y;
            }
        }

        if (g_ddraw.got_child_windows)
            RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);

        return real_DefWindowProcA(hWnd, uMsg, wParam, lParam); /* Carmageddon fix */
    }
    case WM_RESTORE_STYLE:
    {
        if (!IsWine()) /* hack: disable aero snap */
        {
            LONG style = real_GetWindowLongA(g_ddraw.hwnd, GWL_STYLE);

            if (!(style & WS_MAXIMIZEBOX))
            {
                real_SetWindowLongA(g_ddraw.hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX);
            }
        }
        return 0;
    }
    case WM_SYSCOMMAND:
    {
        if ((wParam & ~0x0F) == SC_MOVE && !IsWine()) /* hack: disable aero snap */
        {
            LONG style = real_GetWindowLongA(g_ddraw.hwnd, GWL_STYLE);

            if ((style & WS_MAXIMIZEBOX))
            {
                real_SetWindowLongA(g_ddraw.hwnd, GWL_STYLE, style & ~WS_MAXIMIZEBOX);
                PostMessageA(g_ddraw.hwnd, WM_RESTORE_STYLE, 0, 0);
            }
        }

        if (wParam == SC_MAXIMIZE)
        {
            if (IsWine())
                return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);

            if (g_config.resizable)
            {
                util_toggle_maximize();
            }

            return 0;
        }

        if (wParam == SC_CLOSE && !GameHandlesClose)
        {
            if (g_config.terminate_process)
                g_config.terminate_process = 2;

            ExitProcess(0);
            //_exit(0);
        }

        if (wParam == SC_KEYMENU && GetMenu(g_ddraw.hwnd) == NULL)
            return 0;

        if (!GameHandlesClose)
            return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);

        break;
    }
    case WM_WINDOWPOSCHANGING:
    {
        WINDOWPOS* pos = (WINDOWPOS*)lParam;

        /*
        dbg_dump_swp_flags(pos->flags);
        TRACE(
            "     hwndInsertAfter=%p, x=%d, y=%d, cx=%d, cy=%d\n",
            pos->hwndInsertAfter, pos->x, pos->y, pos->cx, pos->cy);
        */
        
        /* workaround for a bug where sometimes a background window steals the focus */
        if (g_mouse_locked)
        {
            if (pos->flags == SWP_NOMOVE + SWP_NOSIZE)
            {
                mouse_unlock();

                if (real_GetForegroundWindow() == g_ddraw.hwnd)
                    mouse_lock();
            }
        }
        break;
    }
    case WM_MOUSELEAVE:
    {
        //mouse_unlock();
        return 0;
    }
    case WM_ACTIVATE:
    {
        if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE)
        {
            if (g_ddraw.got_child_windows)
                RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }

        if (LOWORD(wParam) == WA_INACTIVE)
        {
            if (g_config.windowed && !g_config.fullscreen && lParam && GetParent((HWND)lParam) == hWnd)
            {
                char class_name[MAX_PATH] = { 0 };
                GetClassNameA((HWND)lParam, class_name, sizeof(class_name) - 1);

                if (strcmp(class_name, "#32770") == 0) // dialog box
                {
                    mouse_unlock();

                    /*
                    // Center to main window
                    RECT rc_main = { 0 };
                    RECT rc_dialog = { 0 };
                    RECT rc = { 0 };

                    real_GetWindowRect(hWnd, &rc_main);
                    real_GetWindowRect((HWND)lParam, &rc_dialog);
                    CopyRect(&rc, &rc_main);

                    OffsetRect(&rc_dialog, -rc_dialog.left, -rc_dialog.top);
                    OffsetRect(&rc, -rc.left, -rc.top);
                    OffsetRect(&rc, -rc_dialog.right, -rc_dialog.bottom);

                    real_SetWindowPos(
                        (HWND)lParam,
                        HWND_TOP,
                        rc_main.left + (rc.right / 2),
                        rc_main.top + (rc.bottom / 2),
                        0, 0,
                        SWP_NOSIZE);
                     */
                }
            }
        }

        if (g_config.windowed || g_config.noactivateapp)
        {
            /* let it pass through once (Atrox) */
            static BOOL one_time;

            if (!one_time)
            {
                one_time = TRUE;

                if (LOWORD(wParam))
                    break;
            }

            return 0;
        }

        break;
    }
    case WM_ACTIVATEAPP:
    {
        if (wParam)
        {
            if (!g_config.windowed)
            {
                if (g_ddraw.renderer != d3d9_render_main || g_config.nonexclusive)
                {
                    ChangeDisplaySettings(&g_ddraw.render.mode, CDS_FULLSCREEN);
                    real_ShowWindow(g_ddraw.hwnd, SW_RESTORE);
                    mouse_lock();
                }
            }
            else if (g_config.fullscreen && real_GetForegroundWindow() == g_ddraw.hwnd)
            {
                mouse_lock();
            }

            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
        }
        else
        {
            if (!g_config.windowed && !g_mouse_locked && g_config.noactivateapp && !g_config.devmode)
                return 0;

            mouse_unlock();

            if (IsWine() && g_ddraw.last_set_window_pos_tick + 500 > timeGetTime())
                return 0;

            if (!g_config.windowed)
            {
                if (g_ddraw.renderer != d3d9_render_main || g_config.nonexclusive)
                {
                    real_ShowWindow(g_ddraw.hwnd, SW_MINIMIZE);
                    ChangeDisplaySettings(NULL, g_ddraw.bnet_active ? CDS_FULLSCREEN : 0);
                }
            }
        }

        if (wParam && g_config.fix_alt_key_stuck)
        {
            INPUT ip;
            memset(&ip, 0, sizeof(ip));

            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = VK_MENU;
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &ip, sizeof(ip));

            if (g_dinput_hook_active)
            {
                ip.type = INPUT_KEYBOARD;
                ip.ki.wScan = 56; // LeftAlt
                ip.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE;
                SendInput(1, &ip, sizeof(ip));
            }
        }

        if (g_config.windowed || g_config.noactivateapp)
        {
            /* let it pass through once (tiberian sun / ClueFinders) */
            static BOOL one_time;
        
            if (!one_time)
            {
                one_time = TRUE;

                if (wParam)
                    break;
            }
            
            if (wParam && g_ddraw.alt_key_down && !g_config.fix_alt_key_stuck)
            {
                PostMessageA(g_ddraw.hwnd, WM_SYSKEYUP, VK_MENU, 0);
                g_ddraw.alt_key_down = FALSE;
            }

            return 0;
        }
        break;
    }
    case WM_AUTORENDERER:
    {
        mouse_unlock();
        real_SetWindowPos(g_ddraw.hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        real_SetWindowPos(g_ddraw.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        mouse_lock();
        return 0;
    }
    case WM_TOGGLE_FULLSCREEN:
    {
        if (!g_config.fullscreen || g_config.toggle_upscaled || (g_config.windowed && g_config.toggle_borderless))
        {
            /* Check if we are fullscreen/borderless already */
            if (wParam == CNC_DDRAW_SET_FULLSCREEN && (!g_config.windowed || g_config.fullscreen))
                return 0;

            /* Check if we are windowed already */
            if (wParam == CNC_DDRAW_SET_WINDOWED && g_config.windowed && !g_config.fullscreen)
                return 0;

            util_toggle_fullscreen();
        }

        return 0;
    }
    case WM_TOGGLE_MAXIMIZE:
    {
        if (g_config.resizable)
        {
            util_toggle_maximize();
        }

        return 0;
    }
    case WM_NCLBUTTONDBLCLK:
    {
        if (g_config.resizable)
        {
            util_toggle_maximize();
        }

        return 0;
    }
    case WM_SYSKEYDOWN:
    {
        if (wParam == VK_F4)
        {
            return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }

        break;
    }
    case WM_SYSKEYUP:
    {
        if (wParam == VK_TAB || (wParam && wParam == g_config.hotkeys.toggle_fullscreen))
        {
            return real_DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }

        break;
    }
    case WM_KEYDOWN:
    {
        break;
    }
    case WM_KEYUP:
    {
        break;
    }
    /* button up messages reactivate cursor lock */
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        if (!g_config.devmode && !g_mouse_locked)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

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

            x = min(x, g_ddraw.width - 1);
            y = min(y, g_ddraw.height - 1);

            InterlockedExchange((LONG*)&g_ddraw.cursor.x, x);
            InterlockedExchange((LONG*)&g_ddraw.cursor.y, y);

            mouse_lock();
            return 0;
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
            return 0;
        }

        if (uMsg == WM_MOUSEWHEEL)
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            real_ScreenToClient(g_ddraw.hwnd, &pt);
            lParam = MAKELPARAM(pt.x, pt.y);
        }

        int x = max(GET_X_LPARAM(lParam) - g_ddraw.mouse.x_adjust, 0);
        int y = max(GET_Y_LPARAM(lParam) - g_ddraw.mouse.y_adjust, 0);

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

        lParam = MAKELPARAM(x, y);

        break;
    }
    case WM_PARENTNOTIFY:
    {
        switch (LOWORD(wParam))
        {
        case WM_DESTROY: /* Workaround for invisible menu on Load/Save/Delete in Tiberian Sun */
            redraw_count = 2;
            break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_XBUTTONDOWN:
        {
            if (!g_config.devmode && !g_mouse_locked)
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);

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

                x = min(x, g_ddraw.width - 1);
                y = min(y, g_ddraw.height - 1);

                InterlockedExchange((LONG*)&g_ddraw.cursor.x, x);
                InterlockedExchange((LONG*)&g_ddraw.cursor.y, y);

                mouse_lock();
            }
            break;
        }
        }
        break;
    }
    case WM_PAINT:
    {
        if (redraw_count > 0)
        {
            redraw_count--;
            RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }

        if (g_ddraw.primary)
        {
            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
        }

        break;
    }
    case WM_ERASEBKGND:
    {
        if (g_ddraw.primary && (g_ddraw.render.viewport.x != 0 || g_ddraw.render.viewport.y != 0))
        {
            InterlockedExchange(&g_ddraw.render.clear_screen, TRUE);
            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
        }
        break;
    }
    }

    return CallWindowProcA(g_ddraw.wndproc, hWnd, uMsg, wParam, lParam);
}
