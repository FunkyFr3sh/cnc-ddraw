#include <windows.h>
#include "ddraw.h"
#include "debug.h"
#include "dd.h"
#include "ddsurface.h"
#include "hook.h"
#include "mouse.h"
#include "render_d3d9.h"
#include "utils.h"
#include "config.h"


void util_limit_game_ticks()
{
    if (g_ddraw->ticks_limiter.htimer)
    {
        FILETIME ft = { 0 };
        GetSystemTimeAsFileTime(&ft);

        if (CompareFileTime((FILETIME*)&g_ddraw->ticks_limiter.due_time, &ft) == -1)
        {
            memcpy(&g_ddraw->ticks_limiter.due_time, &ft, sizeof(LARGE_INTEGER));
        }
        else
        {
            WaitForSingleObject(g_ddraw->ticks_limiter.htimer, g_ddraw->ticks_limiter.tick_length * 2);
        }

        g_ddraw->ticks_limiter.due_time.QuadPart += g_ddraw->ticks_limiter.tick_length_ns;
        SetWaitableTimer(g_ddraw->ticks_limiter.htimer, &g_ddraw->ticks_limiter.due_time, 0, NULL, NULL, FALSE);
    }
    else
    {
        static DWORD next_game_tick;

        if (!next_game_tick)
        {
            next_game_tick = timeGetTime();
            return;
        }

        next_game_tick += g_ddraw->ticks_limiter.tick_length;
        DWORD tick_count = timeGetTime();

        int sleep_time = next_game_tick - tick_count;

        if (sleep_time <= 0 || sleep_time > g_ddraw->ticks_limiter.tick_length)
        {
            next_game_tick = tick_count;
        }
        else
        {
            Sleep(sleep_time);
        }
    }
}

void util_update_bnet_pos(int new_x, int new_y)
{
    static int old_x = -32000;
    static int old_y = -32000;

    if (old_x == -32000 || old_y == -32000 || !g_ddraw->bnet_active)
    {
        old_x = new_x;
        old_y = new_y;
        return;
    }

    POINT pt = { 0, 0 };
    real_ClientToScreen(g_ddraw->hwnd, &pt);

    RECT mainrc;
    SetRect(&mainrc, pt.x, pt.y, pt.x + g_ddraw->width, pt.y + g_ddraw->height);

    int adj_y = 0;
    int adj_x = 0;

    HWND hwnd = FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL);

    while (hwnd != NULL)
    {
        RECT rc;
        real_GetWindowRect(hwnd, &rc);

        OffsetRect(&rc, new_x - old_x, new_y - old_y);

        real_SetWindowPos(
            hwnd,
            0,
            rc.left,
            rc.top,
            0,
            0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

        if (rc.bottom - rc.top <= g_ddraw->height)
        {
            if (rc.bottom > mainrc.bottom && abs(mainrc.bottom - rc.bottom) > abs(adj_y))
            {
                adj_y = mainrc.bottom - rc.bottom;
            }
            else if (rc.top < mainrc.top && abs(mainrc.top - rc.top) > abs(adj_y))
            {
                adj_y = mainrc.top - rc.top;
            }
        }

        if (rc.right - rc.left <= g_ddraw->width)
        {
            if (rc.right > mainrc.right && abs(mainrc.right - rc.right) > abs(adj_x))
            {
                adj_x = mainrc.right - rc.right;
            }
            else if (rc.left < mainrc.left && abs(mainrc.left - rc.left) > abs(adj_x))
            {
                adj_x = mainrc.left - rc.left;
            }
        }

        hwnd = FindWindowEx(HWND_DESKTOP, hwnd, "SDlgDialog", NULL);
    }

    if (adj_x || adj_y)
    {
        HWND hwnd = FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL);

        while (hwnd != NULL)
        {
            RECT rc;
            real_GetWindowRect(hwnd, &rc);

            OffsetRect(&rc, adj_x, adj_y);

            real_SetWindowPos(
                hwnd,
                0,
                rc.left,
                rc.top,
                0,
                0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

            hwnd = FindWindowEx(HWND_DESKTOP, hwnd, "SDlgDialog", NULL);
        }
    }

    old_x = new_x;
    old_y = new_y;
}

BOOL util_get_lowest_resolution(
    float ratio,
    SIZE* out_res,
    DWORD min_width,
    DWORD min_height,
    DWORD max_width,
    DWORD max_height)
{
    BOOL result = FALSE;
    int org_ratio = (int)((ratio + 0.005f) * 10);
    SIZE lowest = { .cx = max_width + 1, .cy = max_height + 1 };
    DWORD i = 0;
    DEVMODE m;
    memset(&m, 0, sizeof(DEVMODE));
    m.dmSize = sizeof(DEVMODE);

    while (EnumDisplaySettings(NULL, i, &m))
    {
        if (m.dmPelsWidth >= min_width &&
            m.dmPelsHeight >= min_height &&
            m.dmPelsWidth <= max_width &&
            m.dmPelsHeight <= max_height &&
            m.dmPelsWidth < lowest.cx &&
            m.dmPelsHeight < lowest.cy)
        {
            int res_ratio = (int)((((float)m.dmPelsWidth / m.dmPelsHeight) + 0.005f) * 10);

            if (res_ratio == org_ratio)
            {
                result = TRUE;
                out_res->cx = lowest.cx = m.dmPelsWidth;
                out_res->cy = lowest.cy = m.dmPelsHeight;
            }
        }

        memset(&m, 0, sizeof(DEVMODE));
        m.dmSize = sizeof(DEVMODE);
        i++;
    }

    return result;
}

void util_toggle_maximize()
{
    RECT client_rc;
    RECT dst_rc;

    LONG style = GetWindowLong(g_ddraw->hwnd, GWL_STYLE);
    LONG exstyle = GetWindowLong(g_ddraw->hwnd, GWL_EXSTYLE);

    if (real_GetClientRect(g_ddraw->hwnd, &client_rc) && SystemParametersInfo(SPI_GETWORKAREA, 0, &dst_rc, 0))
    {
        int width = (dst_rc.right - dst_rc.left);
        int height = (dst_rc.bottom - dst_rc.top);
        int x = dst_rc.left;
        int y = dst_rc.top;

        if (client_rc.right != g_ddraw->width || client_rc.bottom != g_ddraw->height)
        {
            dst_rc.left = 0;
            dst_rc.top = 0;
            dst_rc.right = g_ddraw->width;
            dst_rc.bottom = g_ddraw->height;

            AdjustWindowRectEx(&dst_rc, style, FALSE, exstyle);
        }
        else if (g_ddraw->boxing)
        {
            dst_rc.left = 0;
            dst_rc.top = 0;
            dst_rc.right = g_ddraw->width;
            dst_rc.bottom = g_ddraw->height;

            for (int i = 20; i-- > 1;)
            {
                if (width >= g_ddraw->width * i && height - 20 >= g_ddraw->height * i)
                {
                    dst_rc.right = g_ddraw->width * i;
                    dst_rc.bottom = g_ddraw->height * i;
                    break;
                }
            }

            AdjustWindowRectEx(&dst_rc, style, FALSE, exstyle);
        }
        else if (g_ddraw->maintas)
        {
            util_unadjust_window_rect(&dst_rc, style, FALSE, exstyle);

            int w = dst_rc.right - dst_rc.left;
            int h = dst_rc.bottom - dst_rc.top;

            dst_rc.top = 0;
            dst_rc.left = 0;
            dst_rc.right = w;
            dst_rc.bottom = (LONG)(((float)g_ddraw->height / g_ddraw->width) * w);

            if (dst_rc.bottom > h)
            {
                dst_rc.right = (LONG)(((float)dst_rc.right / dst_rc.bottom) * h);
                dst_rc.bottom = h;
            }

            AdjustWindowRectEx(&dst_rc, style, FALSE, exstyle);
        }

        RECT pos_rc;
        pos_rc.left = (width / 2) - ((dst_rc.right - dst_rc.left) / 2) + x;
        pos_rc.top = (height / 2) - ((dst_rc.bottom - dst_rc.top) / 2) + y;
        pos_rc.right = (dst_rc.right - dst_rc.left);
        pos_rc.bottom = (dst_rc.bottom - dst_rc.top);

        util_unadjust_window_rect(&pos_rc, style, FALSE, exstyle);
        util_unadjust_window_rect(&dst_rc, style, FALSE, exstyle);

        util_set_window_rect(
            pos_rc.left,
            pos_rc.top,
            dst_rc.right - dst_rc.left,
            dst_rc.bottom - dst_rc.top,
            0);
    }
}

void util_toggle_fullscreen()
{
    if (g_ddraw->bnet_active)
        return;

    memset(g_ddraw->primary->bnet_surface, 0, g_ddraw->width * g_ddraw->height);

    if (g_ddraw->toggle_borderless)
    {
        if (!g_ddraw->fullscreen)
        {
            mouse_unlock();

            g_config.borderless_state = g_ddraw->fullscreen = TRUE;
            dd_SetDisplayMode(g_ddraw->width, g_ddraw->height, g_ddraw->bpp, 0);

            mouse_lock();
        }
        else
        {
            mouse_unlock();

            g_config.borderless_state = g_ddraw->fullscreen = FALSE;
            dd_SetDisplayMode(g_ddraw->width, g_ddraw->height, g_ddraw->bpp, 0);

            //mouse_lock();
        }
    }
    else 
    {
        if (g_ddraw->windowed)
        {
            mouse_unlock();

            g_config.window_state = g_ddraw->windowed = FALSE;
            dd_SetDisplayMode(g_ddraw->width, g_ddraw->height, g_ddraw->bpp, SDM_LEAVE_WINDOWED);
            util_update_bnet_pos(0, 0);

            mouse_lock();
        }
        else
        {
            mouse_unlock();
            g_config.window_state = g_ddraw->windowed = TRUE;

            if (g_ddraw->renderer == d3d9_render_main)
            {
                d3d9_reset();
            }
            else
            {
                ChangeDisplaySettings(NULL, g_ddraw->bnet_active ? CDS_FULLSCREEN : 0);
            }

            dd_SetDisplayMode(g_ddraw->width, g_ddraw->height, g_ddraw->bpp, SDM_LEAVE_FULLSCREEN);
            //mouse_lock();
        }
    }
}

BOOL util_unadjust_window_rect(LPRECT prc, DWORD dwStyle, BOOL fMenu, DWORD dwExStyle)
{
    RECT rc;
    SetRectEmpty(&rc);

    BOOL fRc = AdjustWindowRectEx(&rc, dwStyle, fMenu, dwExStyle);

    if (fRc)
    {
        prc->left -= rc.left;
        prc->top -= rc.top;
        prc->right -= rc.right;
        prc->bottom -= rc.bottom;
    }

    return fRc;
}

void util_set_window_rect(int x, int y, int width, int height, UINT flags)
{
    if (g_ddraw->windowed)
    {
        if (g_ddraw->render.thread)
        {
            EnterCriticalSection(&g_ddraw->cs);
            g_ddraw->render.run = FALSE;
            ReleaseSemaphore(g_ddraw->render.sem, 1, NULL);
            LeaveCriticalSection(&g_ddraw->cs);

            WaitForSingleObject(g_ddraw->render.thread, INFINITE);
            g_ddraw->render.thread = NULL;
        }

        if ((flags & SWP_NOMOVE) == 0)
        {
            g_config.window_rect.left = x;
            g_config.window_rect.top = y;
        }

        if ((flags & SWP_NOSIZE) == 0)
        {
            g_config.window_rect.bottom = height;
            g_config.window_rect.right = width;
        }

        dd_SetDisplayMode(g_ddraw->width, g_ddraw->height, g_ddraw->bpp, 0);
    }
}

BOOL CALLBACK util_enum_child_proc(HWND hwnd, LPARAM lparam)
{
    IDirectDrawSurfaceImpl* this = (IDirectDrawSurfaceImpl*)lparam;

    RECT size;
    RECT pos;

    if (real_GetClientRect(hwnd, &size) && real_GetWindowRect(hwnd, &pos) && size.right > 1 && size.bottom > 1)
    {
        //TRACE("     util_enum_child_proc right=%u, bottom=%u\n", size.right, size.bottom);

        if (g_ddraw->fixchilds == FIX_CHILDS_DETECT_HIDE)
        {
            LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);

            if (!(style & WS_EX_TRANSPARENT))
            {
                real_SetWindowLongA(hwnd, GWL_EXSTYLE, style | WS_EX_TRANSPARENT);

                real_SetWindowPos(
                    hwnd,
                    0,
                    0,
                    0,
                    0,
                    0,
                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER
                );
            }
        }
        else
        {
            g_ddraw->got_child_windows = g_ddraw->child_window_exists = TRUE;

            if (g_ddraw->fixchilds == FIX_CHILDS_DETECT_PAINT)
            {
                HDC dst_dc = GetDC(hwnd);
                HDC src_dc;

                dds_GetDC(this, &src_dc);

                real_MapWindowPoints(HWND_DESKTOP, g_ddraw->hwnd, (LPPOINT)&pos, 2);

                BitBlt(dst_dc, 0, 0, size.right, size.bottom, src_dc, pos.left, pos.top, SRCCOPY);

                ReleaseDC(hwnd, dst_dc);
            }
        }
    }

    return FALSE;
}

static unsigned char util_get_pixel(int x, int y)
{
    return ((unsigned char*)dds_GetBuffer(
        g_ddraw->primary))[y * g_ddraw->primary->l_pitch + x * g_ddraw->primary->lx_pitch];
}

BOOL util_detect_low_res_screen()
{
    static int* in_movie = (int*)0x00665F58;
    static int* is_vqa_640 = (int*)0x0065D7BC;
    static BYTE* should_stretch = (BYTE*)0x00607D78;

    if (g_ddraw->width <= g_ddraw->upscale_hack_width || g_ddraw->height <= g_ddraw->upscale_hack_height)
    {
        return FALSE;
    }

    if (g_ddraw->isredalert)
    {
        if ((*in_movie && !*is_vqa_640) || *should_stretch)
        {
            return TRUE;
        }

        return FALSE;
    }
    else if (g_ddraw->iscnc1)
    {
        return
            util_get_pixel(g_ddraw->upscale_hack_width + 1, 0) == 0 ||
            util_get_pixel(g_ddraw->upscale_hack_width + 5, 1) == 0;
    }
    else if (g_ddraw->iskkndx)
    {
        return util_get_pixel(g_ddraw->width - 3, 3) == 0;
    }

    return FALSE;
}
