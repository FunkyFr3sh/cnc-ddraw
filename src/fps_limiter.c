#include <windows.h>
#include "fps_limiter.h"
#include "dd.h"
#include "debug.h"
#include "hook.h"
#include "config.h"

FPSLIMITER g_fpsl;

void fpsl_init()
{
    int max_fps = g_config.maxfps;

    g_fpsl.tick_length_ns = 0;
    g_fpsl.tick_length = 0;

    if (max_fps < 0 || g_config.vsync)
        max_fps = g_ddraw->mode.dmDisplayFrequency;

    if (max_fps > 1000)
        max_fps = 0;

    if (max_fps > 0)
    {
        float len = 1000.0f / max_fps;
        g_fpsl.tick_length_ns = (LONGLONG)(len * 10000);
        g_fpsl.tick_length = (DWORD)len;// + 0.5f;
    }

    if (g_fpsl.got_adapter && g_fpsl.D3DKMTCloseAdapter)
    {
        g_fpsl.initialized = FALSE;
        g_fpsl.got_adapter = FALSE;
        g_fpsl.close_adapter.hAdapter = g_fpsl.adapter.hAdapter;
        g_fpsl.D3DKMTCloseAdapter(&g_fpsl.close_adapter);
    }

    if (!g_fpsl.gdi32_dll)
    {
        g_fpsl.gdi32_dll = real_LoadLibraryA("gdi32.dll");
    }

    if (!g_fpsl.dwmapi_dll)
    {
        g_fpsl.dwmapi_dll = real_LoadLibraryA("dwmapi.dll");
    }

    if (!g_fpsl.DwmFlush)
    {
        g_fpsl.DwmFlush =
            (DWMFLUSHPROC)real_GetProcAddress(g_fpsl.dwmapi_dll, "DwmFlush");
    }

    if (!g_fpsl.DwmIsCompositionEnabled)
    {
        g_fpsl.DwmIsCompositionEnabled =
            (DWMISCOMPOSITIONENABLEDPROC)real_GetProcAddress(g_fpsl.dwmapi_dll, "DwmIsCompositionEnabled");
    }

    if (!g_fpsl.D3DKMTWaitForVerticalBlankEvent)
    {
        g_fpsl.D3DKMTWaitForVerticalBlankEvent =
            (D3DKMTWAITFORVERTICALBLANKEVENTPROC)real_GetProcAddress(g_fpsl.gdi32_dll, "D3DKMTWaitForVerticalBlankEvent");
    }

    if (!g_fpsl.D3DKMTOpenAdapterFromHdc)
    {
        g_fpsl.D3DKMTOpenAdapterFromHdc =
            (D3DKMTOPENADAPTERFROMHDCPROC)real_GetProcAddress(g_fpsl.gdi32_dll, "D3DKMTOpenAdapterFromHdc");
    }

    if (!g_fpsl.D3DKMTCloseAdapter)
    {
        g_fpsl.D3DKMTCloseAdapter =
            (D3DKMTCLOSEADAPTERPROC)real_GetProcAddress(g_fpsl.gdi32_dll, "D3DKMTCloseAdapter");
    }

    g_fpsl.is_wine = real_GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != 0;

    g_fpsl.initialized = TRUE;
}

BOOL fpsl_wait_for_vblank(BOOL open_adapter)
{
    if (g_fpsl.initialized)
    {
        if (open_adapter && g_fpsl.D3DKMTOpenAdapterFromHdc && !g_fpsl.got_adapter)
        {
            g_fpsl.adapter.hDc = g_ddraw->render.hdc;

            if (g_fpsl.D3DKMTOpenAdapterFromHdc(&g_fpsl.adapter) == 0)
            {
                g_fpsl.vblank_event.hAdapter = g_fpsl.adapter.hAdapter;
                g_fpsl.got_adapter = TRUE;
            }
        }

        if (g_fpsl.got_adapter && g_fpsl.D3DKMTWaitForVerticalBlankEvent)
        {
            return g_fpsl.D3DKMTWaitForVerticalBlankEvent(&g_fpsl.vblank_event) == 0;
        }
    }

    return FALSE;
}

BOOL fpsl_dwm_flush()
{
    return 
        g_fpsl.initialized && 
        fpsl_dwm_is_enabled() && 
        g_fpsl.DwmFlush && 
        !g_fpsl.is_wine &&
        SUCCEEDED(g_fpsl.DwmFlush());
}

BOOL fpsl_dwm_is_enabled()
{
    BOOL dwm_enabled = FALSE;

    if (g_fpsl.DwmIsCompositionEnabled)
        g_fpsl.DwmIsCompositionEnabled(&dwm_enabled);

    return dwm_enabled;
}

void fpsl_frame_start()
{
    if (g_fpsl.tick_length > 0)
        g_fpsl.tick_start = timeGetTime();
}

void fpsl_frame_end()
{
    if (g_config.maxfps < 0 || g_config.vsync)
    {
        if (fpsl_dwm_flush() || fpsl_wait_for_vblank(TRUE))
            return;
    }

    if (g_fpsl.tick_length > 0)
    {
        if (g_fpsl.htimer)
        {
            if (g_config.vsync)
            {
                WaitForSingleObject(g_fpsl.htimer, g_fpsl.tick_length * 2);
                LARGE_INTEGER due_time = { .QuadPart = -g_fpsl.tick_length_ns };
                SetWaitableTimer(g_fpsl.htimer, &due_time, 0, NULL, NULL, FALSE);
            }
            else
            {
                FILETIME ft = { 0 };
                GetSystemTimeAsFileTime(&ft);

                if (CompareFileTime((FILETIME*)&g_fpsl.due_time, &ft) == -1)
                {
                    memcpy(&g_fpsl.due_time, &ft, sizeof(LARGE_INTEGER));
                }
                else
                {
                    WaitForSingleObject(g_fpsl.htimer, g_fpsl.tick_length * 2);
                }

                g_fpsl.due_time.QuadPart += g_fpsl.tick_length_ns;
                SetWaitableTimer(g_fpsl.htimer, &g_fpsl.due_time, 0, NULL, NULL, FALSE);
            }
        }
        else
        {
            g_fpsl.tick_end = timeGetTime();

            if (g_fpsl.tick_end - g_fpsl.tick_start < g_fpsl.tick_length)
            {
                Sleep(g_fpsl.tick_length - (g_fpsl.tick_end - g_fpsl.tick_start));
            }
        }
    }
}
