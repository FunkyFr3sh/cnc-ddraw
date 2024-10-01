#include <windows.h>
#include "ddraw.h"
#include "IDirectDraw.h"
#include "dd.h"
#include "hook.h"
#include "config.h"
#include "mouse.h"
#include "wndproc.h"
#include "render_d3d9.h"
#include "render_gdi.h"
#include "render_ogl.h"
#include "fps_limiter.h"
#include "debug.h"
#include "utils.h"
#include "blt.h"


CNCDDRAW* g_ddraw = NULL;

HRESULT dd_EnumDisplayModes(
    DWORD dwFlags,
    LPDDSURFACEDESC lpDDSurfaceDesc,
    LPVOID lpContext,
    LPDDENUMMODESCALLBACK lpEnumModesCallback)
{
    dbg_dump_edm_flags(dwFlags);

    DWORD bpp_filter = 0;

    if (lpDDSurfaceDesc)
    {
        dbg_dump_dds_flags(lpDDSurfaceDesc->dwFlags);
        dbg_dump_dds_caps(lpDDSurfaceDesc->ddsCaps.dwCaps);

        if (lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT)
        {
            TRACE("     ddpfPixelFormat.dwRGBBitCount=%u\n", lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount);

            switch (lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount)
            {
            case 8:
            case 16:
            case 32:
                bpp_filter = lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount;
                break;
            }
        }
    }

    DWORD i = 0;
    DWORD res_count = 0;
    DDSURFACEDESC2 s;

    /* Some games crash when you feed them with too many resolutions so we have to keep the list short */

    DWORD max_w = 0;
    DWORD max_h = 0;
    DEVMODE reg_m;

    memset(&reg_m, 0, sizeof(DEVMODE));
    reg_m.dmSize = sizeof(DEVMODE);

    if (EnumDisplaySettings(NULL, ENUM_REGISTRY_SETTINGS, &reg_m))
    {
        max_w = reg_m.dmPelsWidth;
        max_h = reg_m.dmPelsHeight;
    }

    if (g_config.stronghold_hack && max_w && (max_w % 8))
    {
        while (--max_w % 8);
    }

    BOOL rlf = g_config.resolutions == RESLIST_FULL;
    BOOL rlm = g_config.resolutions == RESLIST_MINI;

    SIZE resolutions[] =
    {
        { 320, 200 },
        { 320, 240 },
        { rlm ? 0 : 512, rlm ? 0 : 384 },
        { 640, 400 },
        { 640, 480 },
        { 800, 600 },
        { 1024, 768 },
        { 1280, 1024 },
        { rlm ? 0 : 1600, rlm ? 0 : 1200 },
        { 1280, 720 },
        { rlf ? 1024 : 0, rlf ? 600 : 0 },
        /* 4:3 */
        { rlf ? 1280 : 0, rlf ? 960 : 0 },
        { rlf ? 2048 : 0, rlf ? 1536 : 0 },
        /* 16:10 */
        { rlf ? 960 : 0, rlf ? 600 : 0 },
        { rlf ? 1440 : 0, rlf ? 900 : 0 },
        { rlf ? 1680 : 0, rlf ? 1050 : 0 },
        { rlf ? 1920 : 0, rlf ? 1200 : 0 },
        { rlf ? 2560 : 0, rlf ? 1600 : 0 },
        /* 16:9 */
        { rlf ? 960 : 0, rlf ? 540 : 0 },
        { rlf ? 1360 : 0, rlf ? 768 : 0 },
        { rlf ? 1600 : 0, rlf ? 900 : 0 },
        { rlf ? 1920 : 0, rlf ? 1080 : 0 },
        { rlf ? 2560 : 0, rlf ? 1440 : 0 },
        /* 21:9 */
        { rlf ? 1280 : 0, rlf ? 540 : 0 },
        { rlf ? 1720 : 0, rlf ? 720 : 0 },
        { rlf ? 2560 : 0, rlf ? 1080 : 0 },
        /* Inject custom resolution */
        { g_config.custom_width, g_config.custom_height },
        { max_w, max_h },
    };
    
    for (int x = 0; x < (sizeof(resolutions) / sizeof(resolutions[0])) - 1; x++)
    {
        if (resolutions[x].cx == max_w && resolutions[x].cy == max_h)
        {
            resolutions[x].cx = 0;
            resolutions[x].cy = 0;
        }
    }

    if ((g_ddraw->bpp && g_config.resolutions == RESLIST_NORMAL) || g_config.resolutions == RESLIST_FULL)
    {
        TRACE("     g_ddraw->bpp=%u\n", g_ddraw->bpp);

        /* set up some filters to keep the list short */
        DWORD refresh_rate = 0;
        DWORD bpp = 0;
        DWORD flags = 99998;
        DWORD fixed_output = 99998;
        DEVMODE m;

        memset(&m, 0, sizeof(DEVMODE));
        m.dmSize = sizeof(DEVMODE);

        while (EnumDisplaySettings(NULL, i, &m))
        {
            TRACE_EXT(
                "     %u: %ux%u@%u %u bpp | flags=0x%08X, FO=%u\n",
                i,
                m.dmPelsWidth,
                m.dmPelsHeight,
                m.dmDisplayFrequency,
                m.dmBitsPerPel,
                m.dmDisplayFlags,
                m.dmDisplayFixedOutput);

            if (refresh_rate != 60 && refresh_rate < 120 && m.dmDisplayFrequency >= 50)
                refresh_rate = m.dmDisplayFrequency;

            /* Some setups with 144hz monitors only contain a very few 60hz resolutions so we can't use 60hz as filter */
            if (m.dmDisplayFrequency > refresh_rate && m.dmDisplayFrequency >= 120)
                refresh_rate = m.dmDisplayFrequency;

            if (bpp != 32 && m.dmBitsPerPel >= 16)
                bpp = m.dmBitsPerPel;

            if (flags != 0)
                flags = m.dmDisplayFlags;

            if (fixed_output != DMDFO_DEFAULT)
                fixed_output = m.dmDisplayFixedOutput;

            memset(&m, 0, sizeof(DEVMODE));
            m.dmSize = sizeof(DEVMODE);
            i++;
        }

        memset(&m, 0, sizeof(DEVMODE));
        m.dmSize = sizeof(DEVMODE);
        i = 0;

        while (EnumDisplaySettings(NULL, i, &m))
        {
            if (refresh_rate == m.dmDisplayFrequency &&
                bpp == m.dmBitsPerPel &&
                flags == m.dmDisplayFlags &&
                fixed_output == m.dmDisplayFixedOutput)
            {
                if (g_config.stronghold_hack && m.dmPelsWidth && (m.dmPelsWidth % 8))
                {
                    while (--m.dmPelsWidth % 8);
                }

                if (i == 0 && g_config.custom_width && g_config.custom_height)
                {
                    m.dmPelsWidth = g_config.custom_width;
                    m.dmPelsHeight = g_config.custom_height;
                }

                TRACE(
                    "     %u: %ux%u@%u %u bpp\n",
                    i,
                    m.dmPelsWidth,
                    m.dmPelsHeight,
                    m.dmDisplayFrequency,
                    m.dmBitsPerPel);

                memset(&s, 0, sizeof(s));

                s.ddpfPixelFormat.dwFlags = DDPF_PALETTEINDEXED8 | DDPF_RGB;
                s.ddpfPixelFormat.dwRGBBitCount = 8;

                s.dwSize = sizeof(DDSURFACEDESC);
                s.dwFlags = DDSD_HEIGHT | DDSD_REFRESHRATE | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT;
                s.dwRefreshRate = 60;
                s.dwHeight = m.dmPelsHeight;
                s.dwWidth = m.dmPelsWidth; 
                s.lPitch = ((s.dwWidth * s.ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;
                s.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);

                if (s.ddpfPixelFormat.dwRGBBitCount == bpp_filter || !bpp_filter)
                {
                    if (g_ddraw->bpp == 8 || g_config.resolutions == RESLIST_FULL)
                    {
                        if (g_config.max_resolutions && res_count++ >= g_config.max_resolutions)
                        {
                            TRACE("     resolution limit reached, stopping\n");
                            return DD_OK;
                        }

                        if (lpEnumModesCallback((LPDDSURFACEDESC)&s, lpContext) == DDENUMRET_CANCEL)
                        {
                            TRACE("     DDENUMRET_CANCEL returned, stopping\n");
                            return DD_OK;
                        }
                    }
                }

                s.ddpfPixelFormat.dwFlags = DDPF_RGB;
                s.ddpfPixelFormat.dwRGBBitCount = 16;
                s.ddpfPixelFormat.dwRBitMask = 0xF800;
                s.ddpfPixelFormat.dwGBitMask = 0x07E0;
                s.ddpfPixelFormat.dwBBitMask = 0x001F;
                s.lPitch = ((s.dwWidth * s.ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;

                if (s.ddpfPixelFormat.dwRGBBitCount == bpp_filter || !bpp_filter)
                {
                    if (g_ddraw->bpp == 16 || g_config.resolutions == RESLIST_FULL)
                    {
                        if (g_config.max_resolutions && res_count++ >= g_config.max_resolutions)
                        {
                            TRACE("     resolution limit reached, stopping\n");
                            return DD_OK;
                        }

                        if (lpEnumModesCallback((LPDDSURFACEDESC)&s, lpContext) == DDENUMRET_CANCEL)
                        {
                            TRACE("     DDENUMRET_CANCEL returned, stopping\n");
                            return DD_OK;
                        }
                    }
                }

                s.ddpfPixelFormat.dwFlags = DDPF_RGB;
                s.ddpfPixelFormat.dwRGBBitCount = 32;
                s.ddpfPixelFormat.dwRBitMask = 0xFF0000;
                s.ddpfPixelFormat.dwGBitMask = 0x00FF00;
                s.ddpfPixelFormat.dwBBitMask = 0x0000FF;
                s.lPitch = ((s.dwWidth * s.ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;

                if (s.ddpfPixelFormat.dwRGBBitCount == bpp_filter || !bpp_filter)
                {
                    if (g_ddraw->bpp == 32 || g_config.resolutions == RESLIST_FULL)
                    {
                        if (g_config.max_resolutions && res_count++ >= g_config.max_resolutions)
                        {
                            TRACE("     resolution limit reached, stopping\n");
                            return DD_OK;
                        }

                        if (lpEnumModesCallback((LPDDSURFACEDESC)&s, lpContext) == DDENUMRET_CANCEL)
                        {
                            TRACE("     DDENUMRET_CANCEL returned, stopping\n");
                            return DD_OK;
                        }
                    }
                }             

                for (int x = 0; x < sizeof(resolutions) / sizeof(resolutions[0]); x++)
                {
                    if (resolutions[x].cx == m.dmPelsWidth && resolutions[x].cy == m.dmPelsHeight)
                    {
                        resolutions[x].cx = 0;
                        resolutions[x].cy = 0;
                    }
                }
            }

            memset(&m, 0, sizeof(DEVMODE));
            m.dmSize = sizeof(DEVMODE);
            i++;
        }
    }

    if (!g_ddraw->bpp || g_config.resolutions != RESLIST_NORMAL)
    {
        for (i = 0; i < sizeof(resolutions) / sizeof(resolutions[0]); i++)
        {
            if (!resolutions[i].cx || !resolutions[i].cy)
                continue;

            if (!(resolutions[i].cx == g_config.custom_width && resolutions[i].cy == g_config.custom_height) &&
                ((max_w && resolutions[i].cx > max_w) || (max_h && resolutions[i].cy > max_h)))
            {
                DEVMODE m;
                memset(&m, 0, sizeof(DEVMODE));

                m.dmSize = sizeof(DEVMODE);
                m.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
                m.dmPelsWidth = resolutions[i].cx;
                m.dmPelsHeight = resolutions[i].cy;
            
                if (ChangeDisplaySettings(&m, CDS_TEST) != DISP_CHANGE_SUCCESSFUL)
                    continue;
            }

            memset(&s, 0, sizeof(s));

            s.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            s.ddpfPixelFormat.dwFlags = DDPF_PALETTEINDEXED8 | DDPF_RGB;
            s.ddpfPixelFormat.dwRGBBitCount = 8;

            s.dwSize = sizeof(DDSURFACEDESC);
            s.dwFlags = DDSD_HEIGHT | DDSD_REFRESHRATE | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT;
            s.dwRefreshRate = 60;
            s.dwHeight = resolutions[i].cy;
            s.dwWidth = resolutions[i].cx;
            s.lPitch = ((s.dwWidth * s.ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;

            if (s.ddpfPixelFormat.dwRGBBitCount == bpp_filter || !bpp_filter)
            {
                if (g_config.max_resolutions && res_count++ >= g_config.max_resolutions)
                {
                    TRACE("     resolution limit reached, stopping\n");
                    return DD_OK;
                }

                if (lpEnumModesCallback((LPDDSURFACEDESC)&s, lpContext) == DDENUMRET_CANCEL)
                {
                    TRACE("     DDENUMRET_CANCEL returned, stopping\n");
                    return DD_OK;
                }
            }

            s.ddpfPixelFormat.dwFlags = DDPF_RGB;
            s.ddpfPixelFormat.dwRGBBitCount = 16;
            s.ddpfPixelFormat.dwRBitMask = 0xF800;
            s.ddpfPixelFormat.dwGBitMask = 0x07E0;
            s.ddpfPixelFormat.dwBBitMask = 0x001F;
            s.lPitch = ((s.dwWidth * s.ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;

            if (s.ddpfPixelFormat.dwRGBBitCount == bpp_filter || !bpp_filter)
            {
                if (g_config.max_resolutions && res_count++ >= g_config.max_resolutions)
                {
                    TRACE("     resolution limit reached, stopping\n");
                    return DD_OK;
                }

                if (lpEnumModesCallback((LPDDSURFACEDESC)&s, lpContext) == DDENUMRET_CANCEL)
                {
                    TRACE("     DDENUMRET_CANCEL returned, stopping\n");
                    return DD_OK;
                }
            }

            if (g_config.resolutions == RESLIST_MINI)
                continue;

            s.ddpfPixelFormat.dwFlags = DDPF_RGB;
            s.ddpfPixelFormat.dwRGBBitCount = 32;
            s.ddpfPixelFormat.dwRBitMask = 0xFF0000;
            s.ddpfPixelFormat.dwGBitMask = 0x00FF00;
            s.ddpfPixelFormat.dwBBitMask = 0x0000FF;
            s.lPitch = ((s.dwWidth * s.ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;

            if (s.ddpfPixelFormat.dwRGBBitCount == bpp_filter || !bpp_filter)
            {
                if (g_config.max_resolutions && res_count++ >= g_config.max_resolutions)
                {
                    TRACE("     resolution limit reached, stopping\n");
                    return DD_OK;
                }

                if (lpEnumModesCallback((LPDDSURFACEDESC)&s, lpContext) == DDENUMRET_CANCEL)
                {
                    TRACE("     DDENUMRET_CANCEL returned, stopping\n");
                    return DD_OK;
                }
            }
        }
    }

    return DD_OK;
}

HRESULT dd_GetCaps(LPDDCAPS_DX1 lpDDDriverCaps, LPDDCAPS_DX1 lpDDEmulCaps)
{
    if (lpDDDriverCaps)
    {
        int size =
            lpDDDriverCaps->dwSize == sizeof(DDCAPS_DX3) ? sizeof(DDCAPS_DX3) : 
            lpDDDriverCaps->dwSize == sizeof(DDCAPS_DX5) ? sizeof(DDCAPS_DX5) : 
            lpDDDriverCaps->dwSize == sizeof(DDCAPS_DX6) ? sizeof(DDCAPS_DX6) : 
            lpDDDriverCaps->dwSize == sizeof(DDCAPS_DX7) ? sizeof(DDCAPS_DX7) : 
            sizeof(DDCAPS_DX1);

        memset(lpDDDriverCaps, 0, size);

        lpDDDriverCaps->dwSize = size;
        lpDDDriverCaps->dwCaps =
            DDCAPS_BLT | 
            DDCAPS_PALETTE | 
            DDCAPS_BLTCOLORFILL | 
            DDCAPS_BLTSTRETCH | 
            DDCAPS_CANCLIP | 
            DDCAPS_CANBLTSYSMEM |
            DDCAPS_CANCLIPSTRETCHED | 
            DDCAPS_COLORKEY;

        lpDDDriverCaps->dwCaps2 =
            DDCAPS2_NOPAGELOCKREQUIRED |
            DDCAPS2_WIDESURFACES;

        lpDDDriverCaps->dwCKeyCaps = 
            DDCKEYCAPS_SRCBLT | 
            DDCKEYCAPS_SRCBLTCLRSPACE;
        
        lpDDDriverCaps->dwFXCaps =
            DDFXCAPS_BLTMIRRORLEFTRIGHT |
            DDFXCAPS_BLTMIRRORUPDOWN;

        lpDDDriverCaps->dwPalCaps = 
            DDPCAPS_8BIT | 
            DDPCAPS_PRIMARYSURFACE;

        lpDDDriverCaps->dwVidMemTotal = 16777216;
        lpDDDriverCaps->dwVidMemFree = 16777216;
        lpDDDriverCaps->ddsCaps.dwCaps = DDSCAPS_FLIP;
    }

    if (lpDDEmulCaps)
    {
        int size =
            lpDDEmulCaps->dwSize == sizeof(DDCAPS_DX3) ? sizeof(DDCAPS_DX3) :
            lpDDEmulCaps->dwSize == sizeof(DDCAPS_DX5) ? sizeof(DDCAPS_DX5) :
            lpDDEmulCaps->dwSize == sizeof(DDCAPS_DX6) ? sizeof(DDCAPS_DX6) :
            lpDDEmulCaps->dwSize == sizeof(DDCAPS_DX7) ? sizeof(DDCAPS_DX7) :
            sizeof(DDCAPS_DX1);

        memset(lpDDEmulCaps, 0, size);

        lpDDEmulCaps->dwSize = size;
    }

    return DD_OK;
}

HRESULT dd_GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc)
{
    if (lpDDSurfaceDesc)
    {
        int size = lpDDSurfaceDesc->dwSize == sizeof(DDSURFACEDESC2) ? sizeof(DDSURFACEDESC2) : sizeof(DDSURFACEDESC);

        memset(lpDDSurfaceDesc, 0, size);

        lpDDSurfaceDesc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_PALETTEINDEXED8 | DDPF_RGB;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = 8;

        lpDDSurfaceDesc->dwSize = size;
        lpDDSurfaceDesc->dwFlags = DDSD_HEIGHT | DDSD_REFRESHRATE | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT;
        lpDDSurfaceDesc->dwRefreshRate = 60;
        lpDDSurfaceDesc->dwHeight = g_ddraw->height ? g_ddraw->height : 768;
        lpDDSurfaceDesc->dwWidth = g_ddraw->width ? g_ddraw->width : 1024;

        lpDDSurfaceDesc->lPitch = 
            ((lpDDSurfaceDesc->dwWidth * lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;

        if (g_ddraw->bpp == 32)
        {
            lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
            lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = 32;
            lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xFF0000;
            lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x00FF00;
            lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x0000FF;

            lpDDSurfaceDesc->lPitch =
                ((lpDDSurfaceDesc->dwWidth * lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;
        }
        else if (g_ddraw->bpp != 8)
        {
            lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
            lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = 16;
            lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xF800;
            lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x07E0;
            lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001F;

            lpDDSurfaceDesc->lPitch =
                ((lpDDSurfaceDesc->dwWidth * lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount + 31) & ~31) >> 3;
        }
    }

    return DD_OK;
}

HRESULT dd_GetMonitorFrequency(LPDWORD lpdwFreq)
{
    if (lpdwFreq)
        *lpdwFreq = 60;

    return DD_OK;
}

HRESULT dd_RestoreDisplayMode()
{
    if (!g_ddraw->render.run)
    {
        return DD_OK;
    }

    if (g_ddraw->renderer != gdi_render_main)
    {
        EnterCriticalSection(&g_ddraw->cs);
        g_ddraw->render.run = FALSE;
        ReleaseSemaphore(g_ddraw->render.sem, 1, NULL);
        LeaveCriticalSection(&g_ddraw->cs);

        if (g_ddraw->render.thread)
        {
            WaitForSingleObject(g_ddraw->render.thread, INFINITE);
            g_ddraw->render.thread = NULL;
        }
    }

    if (!g_config.windowed)
    {
        if (g_ddraw->renderer == d3d9_render_main && !g_config.nonexclusive)
        {
            if (!d3d9_reset(TRUE))
                d3d9_release();
        }
        else
        {
            ChangeDisplaySettings(NULL, 0);
        }
    }

    return DD_OK;
}

HRESULT dd_SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwFlags)
{
    if (dwBPP != 8 && dwBPP != 16 && dwBPP != 32)
        return DDERR_INVALIDMODE;

    if (g_config.mgs_hack && dwHeight == 480) dwHeight -= 32; /* Remove black bar in Metal Gear Solid */

    if (g_ddraw->render.thread)
    {
        EnterCriticalSection(&g_ddraw->cs);
        g_ddraw->render.run = FALSE;
        ReleaseSemaphore(g_ddraw->render.sem, 1, NULL);
        LeaveCriticalSection(&g_ddraw->cs);

        WaitForSingleObject(g_ddraw->render.thread, INFINITE);
        g_ddraw->render.thread = NULL;
    }

    if (!g_ddraw->mode.dmPelsWidth)
    {
        ChangeDisplaySettings(NULL, 0);

        g_ddraw->mode.dmSize = sizeof(DEVMODE);
        g_ddraw->mode.dmDriverExtra = 0;

        if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &g_ddraw->mode) == FALSE)
        {
            g_ddraw->mode.dmSize = sizeof(DEVMODE);
            g_ddraw->mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
            g_ddraw->mode.dmPelsWidth = real_GetSystemMetrics(SM_CXSCREEN);
            g_ddraw->mode.dmPelsHeight = real_GetSystemMetrics(SM_CYSCREEN);
            g_ddraw->mode.dmDisplayFrequency = 60;
            g_ddraw->mode.dmBitsPerPel = 32;

            if (!g_ddraw->mode.dmPelsWidth || !g_ddraw->mode.dmPelsHeight)
            {
                g_config.fullscreen = FALSE;
            }
        }

        if (g_config.maxgameticks == 0 && g_ddraw->mode.dmDisplayFrequency == 60)
        {
            g_config.maxgameticks = -2;
        }
    }

    g_ddraw->render.width = g_config.window_rect.right;
    g_ddraw->render.height = g_config.window_rect.bottom;

    /* temporary fix: center window for games that keep changing their resolution */
    if ((g_ddraw->width || g_config.infantryhack) &&
        (g_ddraw->width != dwWidth || g_ddraw->height != dwHeight) &&
        (dwWidth > g_config.window_rect.right || dwHeight > g_config.window_rect.bottom))
    {
        g_config.window_rect.left = -32000;
        g_config.window_rect.top = -32000;
    }

    g_ddraw->width = dwWidth;
    g_ddraw->height = dwHeight;
    g_ddraw->bpp = dwBPP;

    InterlockedExchange((LONG*)&g_ddraw->cursor.x, dwWidth / 2);
    InterlockedExchange((LONG*)&g_ddraw->cursor.y, dwHeight / 2);

    BOOL border = g_config.border;
    BOOL nonexclusive = FALSE;

    if (g_config.fullscreen)
    {
        g_ddraw->render.width = g_ddraw->mode.dmPelsWidth;
        g_ddraw->render.height = g_ddraw->mode.dmPelsHeight;

        if (g_config.windowed) /* windowed-fullscreen aka borderless */
        {
            border = FALSE;

            /* prevent OpenGL from going automatically into fullscreen exclusive mode */
            if (g_ddraw->renderer == ogl_render_main)
                nonexclusive = TRUE;

        }
    }

    if (g_ddraw->render.width < g_ddraw->width)
    {
        g_ddraw->render.width = g_ddraw->width;
    }

    if (g_ddraw->render.height < g_ddraw->height)
    {
        g_ddraw->render.height = g_ddraw->height;
    }

    g_ddraw->render.run = TRUE;

    BOOL lock_mouse = g_mouse_locked;

    mouse_unlock();

    memset(&g_ddraw->render.mode, 0, sizeof(DEVMODE));
    g_ddraw->render.mode.dmSize = sizeof(DEVMODE);

    if (g_config.refresh_rate)
    {
        g_ddraw->render.mode.dmFields |= DM_DISPLAYFREQUENCY;
        g_ddraw->render.mode.dmDisplayFrequency = g_config.refresh_rate;

        if (ChangeDisplaySettings(&g_ddraw->render.mode, CDS_TEST) != DISP_CHANGE_SUCCESSFUL)
        {
            g_config.refresh_rate = 0;

            g_ddraw->render.mode.dmFields = 0;
            g_ddraw->render.mode.dmDisplayFrequency = 0;
        }
    }

    g_ddraw->render.mode.dmFields |= DM_PELSWIDTH | DM_PELSHEIGHT;
    g_ddraw->render.mode.dmPelsWidth = g_ddraw->render.width;
    g_ddraw->render.mode.dmPelsHeight = g_ddraw->render.height;

    if (!g_config.windowed)
    {
        /* Making sure the chosen resolution is valid */
        if (ChangeDisplaySettings(&g_ddraw->render.mode, CDS_TEST) != DISP_CHANGE_SUCCESSFUL)
        {
            /* Try without upscaling */
            g_ddraw->render.width = g_ddraw->width;
            g_ddraw->render.height = g_ddraw->height;

            g_ddraw->render.mode.dmPelsWidth = g_ddraw->render.width;
            g_ddraw->render.mode.dmPelsHeight = g_ddraw->render.height;

            if (ChangeDisplaySettings(&g_ddraw->render.mode, CDS_TEST) != DISP_CHANGE_SUCCESSFUL)
            {
                /* Try 2x scaling */
                g_ddraw->render.width *= 2;
                g_ddraw->render.height *= 2;

                g_ddraw->render.mode.dmPelsWidth = g_ddraw->render.width;
                g_ddraw->render.mode.dmPelsHeight = g_ddraw->render.height;

                if ((g_ddraw->render.width > g_ddraw->mode.dmPelsWidth ||
                    g_ddraw->render.height > g_ddraw->mode.dmPelsHeight) ||
                    ChangeDisplaySettings(&g_ddraw->render.mode, CDS_TEST) != DISP_CHANGE_SUCCESSFUL)
                {
                    SIZE res = { 0 };

                    /* try to get a resolution with the same aspect ratio as the requested resolution */
                    BOOL found_res = util_get_lowest_resolution(
                        (float)g_ddraw->width / g_ddraw->height,
                        &res,
                        g_ddraw->width + 1, /* don't return the original resolution since we tested that one already */
                        g_ddraw->height,
                        g_ddraw->mode.dmPelsWidth,
                        g_ddraw->mode.dmPelsHeight);

                    if (!found_res)
                    {
                        /* try to get a resolution with the same aspect ratio as the current display mode */
                        found_res = util_get_lowest_resolution(
                            (float)g_ddraw->mode.dmPelsWidth / g_ddraw->mode.dmPelsHeight,
                            &res,
                            g_ddraw->width,
                            g_ddraw->height,
                            g_ddraw->mode.dmPelsWidth,
                            g_ddraw->mode.dmPelsHeight);
                    }

                    g_ddraw->render.width = res.cx;
                    g_ddraw->render.height = res.cy;

                    g_ddraw->render.mode.dmPelsWidth = g_ddraw->render.width;
                    g_ddraw->render.mode.dmPelsHeight = g_ddraw->render.height;

                    if (!found_res ||
                        ChangeDisplaySettings(&g_ddraw->render.mode, CDS_TEST) != DISP_CHANGE_SUCCESSFUL)
                    {
                        if (g_ddraw->width > g_ddraw->mode.dmPelsWidth ||
                            g_ddraw->height > g_ddraw->mode.dmPelsHeight)
                        {
                            /* Downscaling requires adjmouse to be enabled */
                            g_config.adjmouse = TRUE;
                        }

                        /* try current display settings */
                        g_ddraw->render.width = g_ddraw->mode.dmPelsWidth;
                        g_ddraw->render.height = g_ddraw->mode.dmPelsHeight;

                        g_ddraw->render.mode.dmPelsWidth = g_ddraw->render.width;
                        g_ddraw->render.mode.dmPelsHeight = g_ddraw->render.height;

                        if (ChangeDisplaySettings(&g_ddraw->render.mode, CDS_TEST) != DISP_CHANGE_SUCCESSFUL)
                        {
                            /* everything failed, use borderless mode instead */
                            ChangeDisplaySettings(NULL, 0);

                            g_config.windowed = TRUE;
                            g_config.fullscreen = TRUE;
                            border = FALSE;

                            /* prevent OpenGL from going automatically into fullscreen exclusive mode */
                            if (g_ddraw->renderer == ogl_render_main)
                                nonexclusive = TRUE;
                            
                        }
                    }
                }
            }
        }
    }
    
    /* Support downscaling in borderless mode */
    if (g_config.windowed && g_config.fullscreen)
    {
        if (g_ddraw->width > g_ddraw->mode.dmPelsWidth ||
            g_ddraw->height > g_ddraw->mode.dmPelsHeight)
        {
            /* Downscaling requires adjmouse to be enabled */
            g_config.adjmouse = TRUE;

            g_ddraw->render.width = g_ddraw->mode.dmPelsWidth;
            g_ddraw->render.height = g_ddraw->mode.dmPelsHeight;

            g_ddraw->render.mode.dmPelsWidth = g_ddraw->render.width;
            g_ddraw->render.mode.dmPelsHeight = g_ddraw->render.height;
        }
    }

    g_ddraw->render.viewport.width = g_ddraw->render.width;
    g_ddraw->render.viewport.height = g_ddraw->render.height;
    g_ddraw->render.viewport.x = 0;
    g_ddraw->render.viewport.y = 0;

    if (g_config.boxing)
    {
        g_ddraw->render.viewport.width = g_ddraw->width;
        g_ddraw->render.viewport.height = g_ddraw->height;

        for (int i = 20; i-- > 1;)
        {
            if (g_ddraw->width * i <= g_ddraw->render.width && g_ddraw->height * i <= g_ddraw->render.height)
            {
                g_ddraw->render.viewport.width *= i;
                g_ddraw->render.viewport.height *= i;
                break;
            }
        }

        g_ddraw->render.viewport.y = g_ddraw->render.height / 2 - g_ddraw->render.viewport.height / 2;
        g_ddraw->render.viewport.x = g_ddraw->render.width / 2 - g_ddraw->render.viewport.width / 2;
    }
    else if (g_config.maintas)
    {
        g_ddraw->render.viewport.width = g_ddraw->render.width;
        g_ddraw->render.viewport.height =
            (int)(((float)g_ddraw->height / g_ddraw->width) * g_ddraw->render.viewport.width);

        if (g_ddraw->render.viewport.height > g_ddraw->render.height)
        {
            g_ddraw->render.viewport.width =
                (int)(((float)g_ddraw->render.viewport.width / g_ddraw->render.viewport.height) * g_ddraw->render.height);

            g_ddraw->render.viewport.height = g_ddraw->render.height;
        }

        g_ddraw->render.viewport.y = g_ddraw->render.height / 2 - g_ddraw->render.viewport.height / 2;
        g_ddraw->render.viewport.x = g_ddraw->render.width / 2 - g_ddraw->render.viewport.width / 2;
    }

    g_ddraw->render.scale_w = ((float)g_ddraw->render.viewport.width / g_ddraw->width);
    g_ddraw->render.scale_h = ((float)g_ddraw->render.viewport.height / g_ddraw->height);
    g_ddraw->render.unscale_w = ((float)g_ddraw->width / g_ddraw->render.viewport.width);
    g_ddraw->render.unscale_h = ((float)g_ddraw->height / g_ddraw->render.viewport.height);

    g_ddraw->mouse.scale_x = ((float)(g_ddraw->render.viewport.width - 1) / (g_ddraw->width - 1));
    g_ddraw->mouse.scale_y = ((float)(g_ddraw->render.viewport.height - 1) / (g_ddraw->height - 1));
    g_ddraw->mouse.unscale_x = ((float)(g_ddraw->width - 1) / (g_ddraw->render.viewport.width - 1));
    g_ddraw->mouse.unscale_y = ((float)(g_ddraw->height - 1) / (g_ddraw->render.viewport.height - 1));

    g_ddraw->mouse.x_adjust = g_ddraw->render.viewport.x;
    g_ddraw->mouse.y_adjust = g_ddraw->render.viewport.y;

    g_ddraw->mouse.rc.left = g_ddraw->mouse.x_adjust;
    g_ddraw->mouse.rc.top = g_ddraw->mouse.y_adjust;
    g_ddraw->mouse.rc.right = g_ddraw->width + g_ddraw->mouse.x_adjust;
    g_ddraw->mouse.rc.bottom = g_ddraw->height + g_ddraw->mouse.y_adjust;

    if (g_config.adjmouse)
    {
        g_ddraw->mouse.rc.right = g_ddraw->render.viewport.width + g_ddraw->mouse.x_adjust;
        g_ddraw->mouse.rc.bottom = g_ddraw->render.viewport.height + g_ddraw->mouse.y_adjust;
    }

    if (nonexclusive || (g_config.nonexclusive && !g_config.windowed && g_ddraw->renderer == ogl_render_main))
    {
        g_ddraw->render.height++;
        g_ddraw->render.opengl_y_align = 1;
    }
    else
    {
        g_ddraw->render.opengl_y_align = 0;
    }

    if (g_config.windowed)
    {
        if (g_config.remove_menu && GetMenu(g_ddraw->hwnd))
            SetMenu(g_ddraw->hwnd, NULL);

        if (!g_ddraw->wine)
        {
            MSG msg; /* workaround for "Not Responding" window problem in cnc games */
            real_PeekMessageA(&msg, g_ddraw->hwnd, 0, 0, PM_NOREMOVE | PM_QS_INPUT);
        }

        if (!border)
        {
            real_SetWindowLongA(
                g_ddraw->hwnd,
                GWL_STYLE,
                real_GetWindowLongA(
                    g_ddraw->hwnd, 
                    GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU));

            if (GetMenu(g_ddraw->hwnd))
                SetMenu(g_ddraw->hwnd, NULL);         
        }
        else
        {
            real_SetWindowLongA(
                g_ddraw->hwnd,
                GWL_STYLE,
                (real_GetWindowLongA(g_ddraw->hwnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW) & ~WS_MAXIMIZE);
        }

        LONG exstyle = real_GetWindowLongA(g_ddraw->hwnd, GWL_EXSTYLE);

        if ((exstyle & WS_EX_TOOLWINDOW))
        {
            real_SetWindowLongA(g_ddraw->hwnd, GWL_EXSTYLE, exstyle & ~(WS_EX_TOOLWINDOW));
        }

        if (g_ddraw->wine)
        {
            real_SetWindowLongA(
                g_ddraw->hwnd,
                GWL_STYLE,
                (real_GetWindowLongA(g_ddraw->hwnd, GWL_STYLE) | WS_MINIMIZEBOX) & ~(WS_MAXIMIZEBOX | WS_THICKFRAME));
        }

        /* center the window with correct dimensions */
        int cy = g_ddraw->mode.dmPelsWidth ? g_ddraw->mode.dmPelsWidth : g_ddraw->render.width;
        int cx = g_ddraw->mode.dmPelsHeight ? g_ddraw->mode.dmPelsHeight : g_ddraw->render.height;
        int x = (g_config.window_rect.left != -32000) ? g_config.window_rect.left : (cy / 2) - (g_ddraw->render.width / 2);
        int y = (g_config.window_rect.top != -32000) ? g_config.window_rect.top : (cx / 2) - (g_ddraw->render.height / 2);

        if (g_config.fullscreen)
        {
            x = y = 0;
        }
        else if (border && g_config.window_rect.top == -32000 && y < 0)
        {
            /* Make window titlebar visible if window does not fit into screen */
            y = real_GetSystemMetrics(SM_CYCAPTION) + real_GetSystemMetrics(SM_CYSIZEFRAME);
        }

        if (util_is_minimized(g_ddraw->hwnd))
            real_ShowWindow(g_ddraw->hwnd, SW_RESTORE);

        RECT dst = { x, y, g_ddraw->render.width + x, g_ddraw->render.height + y };

        LONG style = real_GetWindowLongA(g_ddraw->hwnd, GWL_STYLE);
        exstyle = real_GetWindowLongA(g_ddraw->hwnd, GWL_EXSTYLE);

        AdjustWindowRectEx(&dst, style, GetMenu(g_ddraw->hwnd) != NULL, exstyle);

        real_SetWindowPos(
            g_ddraw->hwnd, 
            HWND_NOTOPMOST, 
            dst.left, 
            dst.top, 
            (dst.right - dst.left), 
            (dst.bottom - dst.top), 
            SWP_SHOWWINDOW | SWP_FRAMECHANGED);


        BOOL d3d9_active = FALSE;

        if (g_ddraw->renderer == d3d9_render_main)
        {
            d3d9_active = d3d9_create();

            if (!d3d9_active)
            {
                d3d9_release();
                g_ddraw->show_driver_warning = TRUE;
                g_ddraw->renderer = gdi_render_main;
            }
        }

        if (lock_mouse || (g_config.fullscreen && real_GetForegroundWindow() == g_ddraw->hwnd))
            mouse_lock();
    }
    else
    {
        if (GetMenu(g_ddraw->hwnd))
            SetMenu(g_ddraw->hwnd, NULL);

        LONG style = real_GetWindowLongA(g_ddraw->hwnd, GWL_STYLE);

        DWORD swp_flags = SWP_SHOWWINDOW;

        if ((style & WS_CAPTION))
        {
            swp_flags |= SWP_FRAMECHANGED;

            real_SetWindowLongA(
                g_ddraw->hwnd,
                GWL_STYLE,
                style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU));
        }

        LONG exstyle = real_GetWindowLongA(g_ddraw->hwnd, GWL_EXSTYLE);

        if ((exstyle & WS_EX_TOOLWINDOW))
        {
            real_SetWindowLongA(g_ddraw->hwnd, GWL_EXSTYLE, exstyle & ~(WS_EX_TOOLWINDOW));
        }

        BOOL d3d9_active = FALSE;

        if (g_ddraw->renderer == d3d9_render_main)
        {
            if (g_config.nonexclusive)
            {
                if (util_is_minimized(g_ddraw->hwnd))
                    real_ShowWindow(g_ddraw->hwnd, SW_RESTORE);

                real_SetWindowPos(
                    g_ddraw->hwnd,
                    HWND_TOPMOST,
                    0,
                    0,
                    g_ddraw->render.width,
                    g_ddraw->render.height,
                    swp_flags);

                swp_flags = SWP_SHOWWINDOW;
            }

            d3d9_active = d3d9_create();

            if (!d3d9_active)
            {
                d3d9_release();
                g_ddraw->show_driver_warning = TRUE;
                g_ddraw->renderer = gdi_render_main;
            }
        }

        if (!d3d9_active || g_config.nonexclusive)
        {
            if (ChangeDisplaySettings(&g_ddraw->render.mode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
            {
                g_ddraw->render.run = FALSE;
                g_config.windowed = TRUE;
                g_config.fullscreen = TRUE;
                return dd_SetDisplayMode(dwWidth, dwHeight, dwBPP, dwFlags);
            }

            /*
                Fix wayland bug:
                ChangeDisplaySettings fails silently - enable borderless mode in case display resolution was not changed
            */
            if (g_ddraw->wine &&
                (g_ddraw->render.mode.dmPelsWidth != real_GetSystemMetrics(SM_CXSCREEN) ||
                    g_ddraw->render.mode.dmPelsHeight != real_GetSystemMetrics(SM_CYSCREEN)))
            {
                ChangeDisplaySettings(NULL, 0);

                g_ddraw->render.run = FALSE;
                g_config.windowed = TRUE;
                g_config.fullscreen = TRUE;
                return dd_SetDisplayMode(dwWidth, dwHeight, dwBPP, dwFlags);
            }
        }

        if (g_ddraw->wine)
        {
            real_SetWindowLongA(
                g_ddraw->hwnd, 
                GWL_STYLE, 
                real_GetWindowLongA(g_ddraw->hwnd, GWL_STYLE) | WS_MINIMIZEBOX);
        }

        if (util_is_minimized(g_ddraw->hwnd))
            real_ShowWindow(g_ddraw->hwnd, SW_RESTORE);

        real_SetWindowPos(
            g_ddraw->hwnd,
            HWND_TOPMOST,
            0,
            0,
            g_ddraw->render.width,
            g_ddraw->render.height,
            swp_flags);

        if (d3d9_active && g_config.nonexclusive)
            d3d9_reset(TRUE);

        g_ddraw->last_set_window_pos_tick = timeGetTime();

        mouse_lock();
    }

    RedrawWindow(g_ddraw->hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

    if (g_ddraw->render.viewport.x != 0 || g_ddraw->render.viewport.y != 0)
    {
        InterlockedExchange(&g_ddraw->render.clear_screen, TRUE);
    }

    if (g_ddraw->render.thread == NULL)
    {
        InterlockedExchange(&g_ddraw->render.palette_updated, TRUE);
        InterlockedExchange(&g_ddraw->render.surface_updated, TRUE);
        ReleaseSemaphore(g_ddraw->render.sem, 1, NULL);

        g_ddraw->render.thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)g_ddraw->renderer, NULL, 0, NULL);
        SetThreadPriority(g_ddraw->render.thread, THREAD_PRIORITY_ABOVE_NORMAL);
    }

    if ((dwFlags & SDM_MODE_SET_BY_GAME) && !g_config.infantryhack)
    {
        real_SendMessageA(g_ddraw->hwnd, WM_SIZE_DDRAW, 0, MAKELPARAM(g_ddraw->width, g_ddraw->height));
        real_SendMessageA(g_ddraw->hwnd, WM_MOVE_DDRAW, 0, MAKELPARAM(0, 0));
        real_SendMessageA(g_ddraw->hwnd, WM_DISPLAYCHANGE_DDRAW, g_ddraw->bpp, MAKELPARAM(g_ddraw->width, g_ddraw->height));
    }

    return DD_OK;
}

HRESULT dd_SetCooperativeLevel(HWND hwnd, DWORD dwFlags)
{
    dbg_dump_scl_flags(dwFlags);

    if (hwnd == NULL)
    {
        return DD_OK;
    }

    if (g_ddraw->hwnd == NULL)
    {
        g_ddraw->hwnd = hwnd;
    }

    if (!g_ddraw->wndproc)
    {
        hook_init(FALSE);

        g_ddraw->wndproc = (WNDPROC)real_SetWindowLongA(g_ddraw->hwnd, GWL_WNDPROC, (LONG)fake_WndProc);
        g_ddraw->gui_thread_id = GetWindowThreadProcessId(g_ddraw->hwnd, NULL);

        if (!g_ddraw->render.hdc)
        {
            g_ddraw->render.hdc = GetDC(g_ddraw->hwnd);

            PIXELFORMATDESCRIPTOR pfd;
            memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
            pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);

            pfd.nVersion = 1;
            pfd.dwFlags =
                PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER | (g_ddraw->renderer == ogl_render_main ? PFD_SUPPORT_OPENGL : 0);

            pfd.iPixelType = PFD_TYPE_RGBA;
            pfd.cColorBits = g_ddraw->mode.dmBitsPerPel;
            pfd.iLayerType = PFD_MAIN_PLANE;

            SetPixelFormat(g_ddraw->render.hdc, ChoosePixelFormat(g_ddraw->render.hdc, &pfd), &pfd);
        }

        if (!g_config.devmode)
        {
            HCURSOR cursor = real_SetCursor(LoadCursor(NULL, IDC_ARROW));

            if (cursor != LoadCursor(NULL, IDC_WAIT))
                InterlockedExchange((LONG*)&g_ddraw->old_cursor, (LONG)cursor);
        }

        int cursor_count = real_ShowCursor(TRUE) - 1;
        InterlockedExchange((LONG*)&g_ddraw->show_cursor_count, cursor_count);
        real_ShowCursor(FALSE);

        /* Make sure the cursor is visible in windowed mode initially */
        if (g_config.windowed && !g_config.fullscreen && !g_config.devmode && cursor_count < 0)
        {
            while (real_ShowCursor(TRUE) < 0);
        }

        /* Starcraft locks the cursor before ddraw.dll was loaded */
        if (g_config.windowed)
        {
            real_ClipCursor(NULL);
        }

        GetWindowText(g_ddraw->hwnd, (LPTSTR)&g_ddraw->title, sizeof(g_ddraw->title));

        g_ddraw->isredalert = strcmp(g_ddraw->title, "Red Alert") == 0;
        g_ddraw->iscnc1 = strcmp(g_ddraw->title, "Command & Conquer") == 0;
        g_ddraw->iskkndx = strcmp(g_ddraw->title, "KKND Xtreme") == 0;

        if (g_ddraw->iskkndx)
        {
            g_ddraw->upscale_hack_width = 640;
            g_ddraw->upscale_hack_height = 480;
        }
        else if (g_ddraw->isredalert || g_ddraw->iscnc1)
        {
            g_ddraw->upscale_hack_width = 640;
            g_ddraw->upscale_hack_height = 400;
        }

        if (g_config.vhack && !g_ddraw->isredalert && !g_ddraw->iscnc1 && !g_ddraw->iskkndx)
        {
            g_config.vhack = 0;
        }
    }

    /* Infantry Online Zone List Window */
    if (g_config.infantryhack)
    {
        static BOOL windowed, fullscreen;

        if (dwFlags & DDSCL_FULLSCREEN)
        {
            g_config.windowed = windowed;
            g_config.fullscreen = fullscreen;
        }
        else if (dwFlags & DDSCL_NOWINDOWCHANGES)
        {
            windowed = g_config.windowed;
            fullscreen = g_config.fullscreen;

            if (GetMenu(g_ddraw->hwnd) != NULL)
            {
                g_config.windowed = TRUE;
                g_config.fullscreen = FALSE;
            }

            dd_SetDisplayMode(640, 480, 16, SDM_MODE_SET_BY_GAME);
        }
    }

    return DD_OK;
}

HRESULT dd_WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent)
{
    if (g_config.maxgameticks == -2)
    {
        if (fpsl_dwm_flush() || fpsl_wait_for_vblank(g_config.maxfps >= 0 && !g_config.vsync))
            return DD_OK;
    }

    if (!g_ddraw->flip_limiter.tick_length)
        return DD_OK;

    if (g_ddraw->flip_limiter.htimer)
    {
        FILETIME ft = { 0 };
        GetSystemTimeAsFileTime(&ft);

        if (CompareFileTime((FILETIME*)&g_ddraw->flip_limiter.due_time, &ft) == -1)
        {
            memcpy(&g_ddraw->flip_limiter.due_time, &ft, sizeof(LARGE_INTEGER));
        }
        else
        {
            WaitForSingleObject(g_ddraw->flip_limiter.htimer, g_ddraw->flip_limiter.tick_length * 2);
        }

        g_ddraw->flip_limiter.due_time.QuadPart += g_ddraw->flip_limiter.tick_length_ns;
        SetWaitableTimer(g_ddraw->flip_limiter.htimer, &g_ddraw->flip_limiter.due_time, 0, NULL, NULL, FALSE);
    }
    else
    {
        static DWORD next_game_tick;

        if (!next_game_tick)
        {
            next_game_tick = timeGetTime();
            return DD_OK;
        }

        next_game_tick += g_ddraw->flip_limiter.tick_length;
        DWORD tick_count = timeGetTime();

        int sleep_time = next_game_tick - tick_count;

        if (sleep_time <= 0 || sleep_time > g_ddraw->flip_limiter.tick_length)
        {
            next_game_tick = tick_count;
        }
        else
        {
            Sleep(sleep_time);
        }
    }

    return DD_OK;
}

ULONG dd_AddRef()
{
    return ++g_ddraw->ref;
}

ULONG dd_Release()
{
    g_ddraw->ref--;

    if (g_ddraw->ref == 0)
    {
        if (g_ddraw->bpp)
        {
            cfg_save();
        }

        if (g_ddraw->render.run)
        {
            EnterCriticalSection(&g_ddraw->cs);
            g_ddraw->render.run = FALSE;
            ReleaseSemaphore(g_ddraw->render.sem, 1, NULL);
            LeaveCriticalSection(&g_ddraw->cs);

            if (g_ddraw->render.thread)
            {
                WaitForSingleObject(g_ddraw->render.thread, INFINITE);
                g_ddraw->render.thread = NULL;
            }
        }

        if (!g_config.windowed)
        {
            if (g_ddraw->renderer == d3d9_render_main && !g_config.nonexclusive)
            {
                if (!d3d9_reset(TRUE))
                    d3d9_release();
            }
            else
            {
                ChangeDisplaySettings(NULL, 0);
            }
        }

        if (g_ddraw->render.hdc)
        {
            ReleaseDC(g_ddraw->hwnd, g_ddraw->render.hdc);
            g_ddraw->render.hdc = NULL;
        }

        if (g_ddraw->ticks_limiter.htimer)
        {
            CancelWaitableTimer(g_ddraw->ticks_limiter.htimer);
            CloseHandle(g_ddraw->ticks_limiter.htimer);
            g_ddraw->ticks_limiter.htimer = NULL;
        }

        if (g_ddraw->flip_limiter.htimer)
        {
            CancelWaitableTimer(g_ddraw->flip_limiter.htimer);
            CloseHandle(g_ddraw->flip_limiter.htimer);
            g_ddraw->flip_limiter.htimer = NULL;
        }

        if (g_fpsl.htimer)
        {
            CancelWaitableTimer(g_fpsl.htimer);
            CloseHandle(g_fpsl.htimer);
            g_fpsl.htimer = NULL;
        }

        if (g_ddraw->real_dd)
        {
            g_ddraw->real_dd->lpVtbl->Release(g_ddraw->real_dd);
        }

        DeleteCriticalSection(&g_ddraw->cs);

        /* restore old wndproc, subsequent ddraw creation will otherwise fail */
        if (g_ddraw->hwnd)
        {
            real_SetWindowLongA(g_ddraw->hwnd, GWL_WNDPROC, (LONG)g_ddraw->wndproc);
        }

        HeapFree(GetProcessHeap(), 0, g_ddraw);
        g_ddraw = NULL;

        return 0;
    }

    return g_ddraw->ref;
}

HRESULT dd_GetAvailableVidMem(LPDDSCAPS lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree)
{
    if (lpdwTotal)
        *lpdwTotal = 16777216;

    if (lpdwFree)
        *lpdwFree = 16777216;

    return DD_OK;
}

HRESULT dd_GetVerticalBlankStatus(LPBOOL lpbIsInVB)
{
    if (lpbIsInVB)
        *lpbIsInVB = TRUE;

    return DD_OK;
}

HRESULT dd_GetDeviceIdentifier(LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags, REFIID riid)
{
    if (!pDDDI)
        return DDERR_INVALIDPARAMS;

    if (IsEqualGUID(&IID_IDirectDraw7, riid))
    {
        memset(pDDDI, 0, sizeof(DDDEVICEIDENTIFIER2));
    }
    else
    {
        memset(pDDDI, 0, sizeof(DDDEVICEIDENTIFIER));
    }
    
    return DD_OK;
}

HRESULT dd_CreateEx(GUID* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown* pUnkOuter)
{
    if (!g_ddraw)
    {
        g_ddraw = (CNCDDRAW*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CNCDDRAW));
        g_ddraw->ref++;

        InitializeCriticalSection(&g_ddraw->cs);

        g_ddraw->render.sem = CreateSemaphore(NULL, 0, 1, NULL);
        g_ddraw->wine = real_GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != 0;
        g_blt_use_avx = util_is_avx_supported();

        if (g_config.minfps > 1000)
            g_config.minfps = 1000;

        if (g_config.minfps > 0)
            g_ddraw->minfps_tick_len = (DWORD)(1000.0f / g_config.minfps);

        /* can't fully set it up here due to missing g_ddraw->mode.dmDisplayFrequency  */
        g_fpsl.htimer = CreateWaitableTimer(NULL, TRUE, NULL);

        if (g_config.maxgameticks > 0 && g_config.maxgameticks <= 1000)
        {
            g_ddraw->ticks_limiter.htimer = CreateWaitableTimer(NULL, TRUE, NULL);

            float len = 1000.0f / g_config.maxgameticks;
            g_ddraw->ticks_limiter.tick_length_ns = (LONGLONG)(len * 10000);
            g_ddraw->ticks_limiter.tick_length = (DWORD)(len + 0.5f);
        }

        if (g_config.maxgameticks >= 0 || g_config.maxgameticks == -2)
        {
            /* always using 60 fps for flip...  */
            g_ddraw->flip_limiter.htimer = CreateWaitableTimer(NULL, TRUE, NULL);

            float flip_len = 1000.0f / 60;
            g_ddraw->flip_limiter.tick_length_ns = (LONGLONG)(flip_len * 10000);
            g_ddraw->flip_limiter.tick_length = (DWORD)(flip_len + 0.5f);
        }


        DWORD system_affinity;
        DWORD proc_affinity;
        HANDLE proc = GetCurrentProcess();

        if (g_config.singlecpu)
        {
            SetProcessAffinityMask(proc, 1);
        }
        else if (GetProcessAffinityMask(proc, &proc_affinity, &system_affinity))
        {
            SetProcessAffinityMask(proc, system_affinity);
        }

        if (GetProcessAffinityMask(proc, &proc_affinity, &system_affinity))
        {
            TRACE("     proc_affinity=%08X, system_affinity=%08X\n", proc_affinity, system_affinity);
        }


        if (_strcmpi(g_config.renderer, "direct3d9on12") == 0)
        {
            g_ddraw->d3d9on12 = TRUE;
        }
        else if (_strcmpi(g_config.renderer, "openglcore") == 0)
        {
            g_ddraw->opengl_core = TRUE;
        }

        if (tolower(g_config.renderer[0]) == 'd') /* direct3d9 or direct3d9on12*/
        {
            g_ddraw->renderer = d3d9_render_main;
        }
        else if (tolower(g_config.renderer[0]) == 's' || tolower(g_config.renderer[0]) == 'g') /* gdi */
        {
            g_ddraw->renderer = gdi_render_main;
        }
        else if (tolower(g_config.renderer[0]) == 'o') /* opengl or openglcore */
        {
            if (oglu_load_dll())
            {
                g_ddraw->renderer = ogl_render_main;
            }
            else
            {
                g_ddraw->show_driver_warning = TRUE;
                g_ddraw->renderer = gdi_render_main;
            }
        }
        else /* auto */
        {
            if (!g_ddraw->wine && d3d9_is_available())
            {
                g_ddraw->renderer = d3d9_render_main;
            }
            else if (oglu_load_dll())
            {
                g_ddraw->renderer = ogl_render_main;
            }
            else
            {
                g_ddraw->show_driver_warning = TRUE;
                g_ddraw->renderer = gdi_render_main;
            }
        }

        g_ddraw->ref--;
    }

    IDirectDrawImpl* dd = (IDirectDrawImpl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawImpl));
    memcpy(&dd->guid, iid, sizeof(dd->guid));

    if (IsEqualGUID(&IID_IDirectDraw, iid))
    {
        TRACE("     GUID = %08X (IID_IDirectDraw), ddraw = %p\n", ((GUID*)iid)->Data1, dd);

        dd->lpVtbl = &g_dd_vtbl1;
    }
    else
    {
        TRACE("     GUID = %08X (IID_IDirectDrawX), ddraw = %p\n", ((GUID*)iid)->Data1, dd);

        dd->lpVtbl = &g_dd_vtblx;
    }

    IDirectDraw_AddRef(dd);

    *lplpDD = (LPVOID)dd;

    return DD_OK;
}
