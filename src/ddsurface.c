#include <windows.h>
#include <stdio.h>
#include "dllmain.h"
#include "dd.h"
#include "hook.h"
#include "ddsurface.h"
#include "mouse.h"
#include "IDirectDrawSurface.h"
#include "winapi_hooks.h"
#include "debug.h"
#include "utils.h"
#include "blt.h"
#include "config.h"
#include "ddclipper.h"
#include "versionhelpers.h"


LONG g_dds_gdi_handles;

HRESULT dds_AddAttachedSurface(IDirectDrawSurfaceImpl* This, IDirectDrawSurfaceImpl* lpDDSurface)
{
    if (lpDDSurface)
    {
        IDirectDrawSurface_AddRef(lpDDSurface);

        if (!This->backbuffer)
        {
            if (This->caps & DDSCAPS_FRONTBUFFER)
            {
                lpDDSurface->caps |= DDSCAPS_BACKBUFFER;
            }

            lpDDSurface->caps |= DDSCAPS_FLIP;

            This->backbuffer = lpDDSurface;
        }
    }

    return DD_OK;
}

HRESULT dds_Blt(
    IDirectDrawSurfaceImpl* This,
    LPRECT lpDestRect,
    IDirectDrawSurfaceImpl* lpDDSrcSurface,
    LPRECT lpSrcRect,
    DWORD dwFlags,
    LPDDBLTFX lpDDBltFx)
{
    dbg_dump_dds_blt_flags(dwFlags);
    dbg_dump_dds_blt_fx_flags((dwFlags & DDBLT_DDFX) && lpDDBltFx ? lpDDBltFx->dwDDFX : 0);

    if (g_ddraw.ref && 
        g_ddraw.iskkndx &&
        (dwFlags & DDBLT_COLORFILL) &&
        lpDestRect &&
        lpDestRect->right == 640 &&
        lpDestRect->bottom == 480)
    {
        if (This->backbuffer)
        {
            dds_Blt(This->backbuffer, lpDestRect, NULL, NULL, dwFlags, lpDDBltFx);
        }

        lpDestRect = NULL;
    }

    IDirectDrawSurfaceImpl* src_surface = lpDDSrcSurface;

    RECT src_rect = { 0, 0, src_surface ? src_surface->width : 0, src_surface ? src_surface->height : 0 };
    RECT dst_rect = { 0, 0, This->width, This->height };

    if (lpSrcRect && src_surface)
        memcpy(&src_rect, lpSrcRect, sizeof(src_rect));

    if (lpDestRect)
        memcpy(&dst_rect, lpDestRect, sizeof(dst_rect));

    int src_w = src_rect.right - src_rect.left;
    int src_h = src_rect.bottom - src_rect.top;

    int dst_w = dst_rect.right - dst_rect.left;
    int dst_h = dst_rect.bottom - dst_rect.top;

    float scale_w = (src_w > 0 && dst_w > 0) ? (float)src_w / dst_w : 1.0f;
    float scale_h = (src_h > 0 && dst_h > 0) ? (float)src_h / dst_h : 1.0f;

    BOOL is_stretch_blt = src_w != dst_w || src_h != dst_h;

    if (This->clipper && !This->clipper->hwnd && !(dwFlags & DDBLT_NO_CLIP) && dst_w > 0 && dst_h > 0)
    {
        DWORD size = 0;

        HRESULT result = ddc_GetClipList(This->clipper, &dst_rect, NULL, &size);

        if (SUCCEEDED(result))
        {
            RGNDATA* list = (RGNDATA*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);

            if (list)
            {
                if (SUCCEEDED(ddc_GetClipList(This->clipper, &dst_rect, list, &size)))
                {
                    RECT* dst_c_rect = (RECT*)list->Buffer;

                    for (int i = 0; i < list->rdh.nCount; ++i)
                    {
                        RECT src_c_rect = src_rect;

                        if (src_surface)
                        {
                            src_c_rect.left += (LONG)((dst_c_rect[i].left - dst_rect.left) * scale_w);
                            src_c_rect.top += (LONG)((dst_c_rect[i].top - dst_rect.top) * scale_h);
                            src_c_rect.right -= (LONG)((dst_rect.right - dst_c_rect[i].right) * scale_w);
                            src_c_rect.bottom -= (LONG)((dst_rect.bottom - dst_c_rect[i].bottom) * scale_h);
                        }

                        dds_Blt(This, &dst_c_rect[i], src_surface, &src_c_rect, dwFlags | DDBLT_NO_CLIP, lpDDBltFx);
                    }
                }

                HeapFree(GetProcessHeap(), 0, list);

                return DD_OK;
            }
        }
        else if (result == DDERR_NOCLIPLIST)
        {
            TRACE("     DDERR_NOCLIPLIST\n");
            //return DDERR_NOCLIPLIST;
        }
        else
        {
            TRACE("     DDERR_INVALIDCLIPLIST\n");
            //return DDERR_INVALIDCLIPLIST;
        }
    }

    if (dst_rect.right < 0)
        dst_rect.right = 0;

    if (dst_rect.left < 0)
    {
        src_rect.left += (LONG)(abs(dst_rect.left) * scale_w);
        dst_rect.left = 0;
    }

    if (dst_rect.bottom < 0)
        dst_rect.bottom = 0;

    if (dst_rect.top < 0)
    {
        src_rect.top += (LONG)(abs(dst_rect.top) * scale_h);
        dst_rect.top = 0;
    }

    if (dst_rect.right > This->width)
    {
        src_rect.right -= (LONG)((dst_rect.right - This->width) * scale_w);
        dst_rect.right = This->width;
    }

    if (dst_rect.left > dst_rect.right)
        dst_rect.left = dst_rect.right;

    if (dst_rect.bottom > This->height)
    {
        src_rect.bottom -= (LONG)((dst_rect.bottom - This->height) * scale_h);
        dst_rect.bottom = This->height;
    }

    if (dst_rect.top > dst_rect.bottom)
        dst_rect.top = dst_rect.bottom;

    if (src_surface)
    {
        if (src_rect.right < 0)
            src_rect.right = 0;

        if (src_rect.left < 0)
            src_rect.left = 0;

        if (src_rect.bottom < 0)
            src_rect.bottom = 0;

        if (src_rect.top < 0)
            src_rect.top = 0;

        if (src_rect.right > src_surface->width)
            src_rect.right = src_surface->width;

        if (src_rect.left > src_rect.right)
            src_rect.left = src_rect.right;

        if (src_rect.bottom > src_surface->height)
            src_rect.bottom = src_surface->height;

        if (src_rect.top > src_rect.bottom)
            src_rect.top = src_rect.bottom;
    }

    src_w = src_rect.right - src_rect.left;
    src_h = src_rect.bottom - src_rect.top;

    int src_x = src_rect.left;
    int src_y = src_rect.top;

    dst_w = dst_rect.right - dst_rect.left;
    dst_h = dst_rect.bottom - dst_rect.top;

    int dst_x = dst_rect.left;
    int dst_y = dst_rect.top;

    void* dst_buf = dds_GetBuffer(This);
    void* src_buf = dds_GetBuffer(src_surface);

    if (dst_buf && (dwFlags & DDBLT_COLORFILL) && lpDDBltFx && dst_w > 0 && dst_h > 0)
    {
        blt_colorfill(
            dst_buf,
            dst_x,
            dst_y,
            dst_w,
            dst_h,
            This->pitch,
            lpDDBltFx->dwFillColor,
            This->bpp);
    }

    if (src_surface && src_w > 0 && src_h > 0 && dst_w > 0 && dst_h > 0)
    {
        if (!is_stretch_blt)
        {
            src_w = dst_w = min(src_w, dst_w);
            src_h = dst_h = min(src_h, dst_h);
        }

        BOOL got_fx = (dwFlags & DDBLT_DDFX) && lpDDBltFx;
        BOOL mirror_left_right = got_fx && (lpDDBltFx->dwDDFX & DDBLTFX_MIRRORLEFTRIGHT);
        BOOL mirror_up_down = got_fx && (lpDDBltFx->dwDDFX & DDBLTFX_MIRRORUPDOWN);

        if (This->bpp != src_surface->bpp)
        {
            TRACE_EXT("     NOT_IMPLEMENTED This->bpp=%u, src_surface->bpp=%u\n", This->bpp, src_surface->bpp);

            HDC dst_dc;
            dds_GetDC(This, &dst_dc);

            HDC src_dc;
            dds_GetDC(src_surface, &src_dc);

            if ((dwFlags & DDBLT_KEYSRC) || (dwFlags & DDBLT_KEYSRCOVERRIDE))
            {
                UINT color = 
                    (dwFlags & DDBLT_KEYSRCOVERRIDE) ?
                    lpDDBltFx->ddckSrcColorkey.dwColorSpaceLowValue : src_surface->color_key.dwColorSpaceLowValue;

                if (src_surface->bpp == 32)
                {
                    color = color & 0xFFFFFF;
                }
                else if (src_surface->bpp == 16)
                {
                    unsigned short c = (unsigned short)color;

                    BYTE r = ((c & 0xF800) >> 11) << 3;
                    BYTE g = ((c & 0x07E0) >> 5) << 2;
                    BYTE b = ((c & 0x001F)) << 3;

                    color = RGB(r, g, b);
                }
                else if (src_surface->bpp == 8)
                {
                    RGBQUAD* quad =
                        src_surface->palette ? src_surface->palette->data_rgb :
                        g_ddraw.ref && g_ddraw.primary && g_ddraw.primary->palette ? g_ddraw.primary->palette->data_rgb :
                        NULL;

                    if (quad)
                    {
                        unsigned char i = (unsigned char)color;

                        color = RGB(quad[i].rgbRed, quad[i].rgbGreen, quad[i].rgbBlue);
                    }                   
                }

                GdiTransparentBlt(dst_dc, dst_x, dst_y, dst_w, dst_h, src_dc, src_x, src_y, src_w, src_h, color);
            }
            else
            {
                real_StretchBlt(dst_dc, dst_x, dst_y, dst_w, dst_h, src_dc, src_x, src_y, src_w, src_h, SRCCOPY);
            }

            /*
            StretchBlt(
                dst_dc, 
                lpDestRect->left, 
                lpDestRect->top, 
                lpDestRect->right - lpDestRect->left, 
                lpDestRect->bottom - lpDestRect->top, 
                src_dc, 
                lpSrcRect->left, 
                lpSrcRect->top, 
                lpSrcRect->right - lpSrcRect->left, 
                lpSrcRect->bottom - lpSrcRect->top, 
                SRCCOPY);
                */
        }
        else if (
            (dwFlags & DDBLT_KEYSRC) ||
            (dwFlags & DDBLT_KEYSRCOVERRIDE) ||
            mirror_left_right ||
            mirror_up_down)
        {
            DDCOLORKEY color_key = { 0xFFFFFFFF, 0 };

            if ((dwFlags & DDBLT_KEYSRC) || (dwFlags & DDBLT_KEYSRCOVERRIDE))
            {
                color_key.dwColorSpaceLowValue =
                    (dwFlags & DDBLT_KEYSRCOVERRIDE) ?
                    lpDDBltFx->ddckSrcColorkey.dwColorSpaceLowValue : src_surface->color_key.dwColorSpaceLowValue;

                color_key.dwColorSpaceHighValue =
                    (dwFlags & DDBLT_KEYSRCOVERRIDE) ?
                    lpDDBltFx->ddckSrcColorkey.dwColorSpaceHighValue : src_surface->color_key.dwColorSpaceHighValue;

                if (color_key.dwColorSpaceHighValue < color_key.dwColorSpaceLowValue)
                    color_key.dwColorSpaceHighValue = color_key.dwColorSpaceLowValue;
            }

            if (!is_stretch_blt && !mirror_left_right && !mirror_up_down)
            {
                blt_colorkey(
                    dst_buf,
                    dst_x,
                    dst_y,
                    dst_w,
                    dst_h,
                    This->pitch,
                    src_buf,
                    src_x,
                    src_y,
                    src_surface->pitch,
                    color_key.dwColorSpaceLowValue,
                    color_key.dwColorSpaceHighValue,
                    This->bpp);
            }
            else
            {
                blt_colorkey_mirror_stretch(
                    dst_buf,
                    dst_x,
                    dst_y,
                    dst_w,
                    dst_h,
                    This->pitch,
                    src_buf,
                    src_x,
                    src_y,
                    src_w,
                    src_h,
                    src_surface->pitch,
                    color_key.dwColorSpaceLowValue,
                    color_key.dwColorSpaceHighValue,
                    mirror_up_down,
                    mirror_left_right,
                    This->bpp);
            }
        }
        else if (is_stretch_blt && (src_w != dst_w || src_h != dst_h))
        {
            blt_stretch(
                dst_buf,
                dst_x,
                dst_y,
                dst_w,
                dst_h,
                This->pitch,
                src_buf,
                src_x,
                src_y,
                src_w,
                src_h,
                src_surface->pitch,
                This->bpp);
        }
        else if (This == src_surface)
        {
            blt_overlap(
                dst_buf,
                dst_x,
                dst_y,
                dst_w,
                dst_h,
                This->pitch,
                src_buf,
                src_x,
                src_y,
                src_surface->pitch,
                This->bpp);
        }
        else
        {
            blt_clean(
                dst_buf,
                dst_x,
                dst_y,
                dst_w,
                dst_h,
                This->pitch,
                src_buf,
                src_x,
                src_y,
                src_surface->pitch,
                This->bpp);
        }
    }

    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && g_ddraw.ref && g_ddraw.render.run)
    {
        if (!(dwFlags & DDBLT_COLORFILL))
            InterlockedExchange(&g_ddraw.zoom.frame_skip, 0);

        InterlockedExchange(&g_ddraw.render.surface_updated, TRUE);

        if (!(This->flags & DDSD_BACKBUFFERCOUNT) || This->last_flip_tick + FLIP_REDRAW_TIMEOUT < timeGetTime())
        {
            This->last_blt_tick = timeGetTime();

            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
            SwitchToThread();

            if (g_ddraw.ticks_limiter.tick_length > 0)
            {
                g_ddraw.ticks_limiter.dds_unlock_limiter_disabled = TRUE;
                util_limit_game_ticks();
            }
        }
    }

    return DD_OK;
}

HRESULT dds_BltFast(
    IDirectDrawSurfaceImpl* This,
    DWORD dwX,
    DWORD dwY,
    IDirectDrawSurfaceImpl* lpDDSrcSurface,
    LPRECT lpSrcRect,
    DWORD dwFlags)
{
    dbg_dump_dds_blt_fast_flags(dwFlags);

    IDirectDrawSurfaceImpl* src_surface = lpDDSrcSurface;

    RECT src_rect = { 0, 0, src_surface ? src_surface->width : 0, src_surface ? src_surface->height : 0 };

    if (lpSrcRect && src_surface)
        memcpy(&src_rect, lpSrcRect, sizeof(src_rect));

    int dst_x = dwX;
    int dst_y = dwY;

    if (dst_x < 0)
    {
        src_rect.left += abs(dst_x);
        dst_x = 0;
    }

    if (dst_y < 0)
    {
        src_rect.top += abs(dst_y);
        dst_y = 0;
    }

    if (src_surface)
    {
        if (src_rect.right < 0)
            src_rect.right = 0;

        if (src_rect.left < 0)
            src_rect.left = 0;

        if (src_rect.bottom < 0)
            src_rect.bottom = 0;

        if (src_rect.top < 0)
            src_rect.top = 0;

        if (src_rect.right > src_surface->width)
            src_rect.right = src_surface->width;

        if (src_rect.left > src_rect.right)
            src_rect.left = src_rect.right;

        if (src_rect.bottom > src_surface->height)
            src_rect.bottom = src_surface->height;

        if (src_rect.top > src_rect.bottom)
            src_rect.top = src_rect.bottom;
    }

    int src_x = src_rect.left;
    int src_y = src_rect.top;

    RECT dst_rect = { dst_x, dst_y, (src_rect.right - src_rect.left) + dst_x, (src_rect.bottom - src_rect.top) + dst_y };

    if (dst_rect.right < 0)
        dst_rect.right = 0;

    if (dst_rect.left < 0)
        dst_rect.left = 0;

    if (dst_rect.bottom < 0)
        dst_rect.bottom = 0;

    if (dst_rect.top < 0)
        dst_rect.top = 0;

    if (dst_rect.right > This->width)
        dst_rect.right = This->width;

    if (dst_rect.left > dst_rect.right)
        dst_rect.left = dst_rect.right;

    if (dst_rect.bottom > This->height)
        dst_rect.bottom = This->height;

    if (dst_rect.top > dst_rect.bottom)
        dst_rect.top = dst_rect.bottom;

    dst_x = dst_rect.left;
    dst_y = dst_rect.top;

    int dst_w = dst_rect.right - dst_rect.left;
    int dst_h = dst_rect.bottom - dst_rect.top;

    void* dst_buf = dds_GetBuffer(This);
    void* src_buf = dds_GetBuffer(src_surface);

    if (src_surface && dst_w > 0 && dst_h > 0)
    {
        if (This->bpp != src_surface->bpp)
        {
            TRACE_EXT("     NOT_IMPLEMENTED This->bpp=%u, src_surface->bpp=%u\n", This->bpp, src_surface->bpp);

            HDC dst_dc;
            dds_GetDC(This, &dst_dc);

            HDC src_dc;
            dds_GetDC(src_surface, &src_dc);

            if (dwFlags & DDBLTFAST_SRCCOLORKEY)
            {
                UINT color = src_surface->color_key.dwColorSpaceLowValue;

                if (src_surface->bpp == 32)
                {
                    color = color & 0xFFFFFF;
                }
                else if (src_surface->bpp == 16)
                {
                    unsigned short c = (unsigned short)color;

                    BYTE r = ((c & 0xF800) >> 11) << 3;
                    BYTE g = ((c & 0x07E0) >> 5) << 2;
                    BYTE b = ((c & 0x001F)) << 3;

                    color = RGB(r, g, b);
                }
                else if (src_surface->bpp == 8)
                {
                    RGBQUAD* quad =
                        src_surface->palette ? src_surface->palette->data_rgb :
                        g_ddraw.ref && g_ddraw.primary && g_ddraw.primary->palette ? g_ddraw.primary->palette->data_rgb :
                        NULL;

                    if (quad)
                    {
                        unsigned char i = (unsigned char)color;

                        color = RGB(quad[i].rgbRed, quad[i].rgbGreen, quad[i].rgbBlue);
                    }
                }

                GdiTransparentBlt(dst_dc, dst_x, dst_y, dst_w, dst_h, src_dc, src_x, src_y, dst_w, dst_h, color);
            }
            else
            {
                real_BitBlt(dst_dc, dst_x, dst_y, dst_w, dst_h, src_dc, src_x, src_y, SRCCOPY);
            }

            /*
            real_BitBlt(
                dst_dc, 
                dwX, 
                dwY, 
                lpSrcRect->right - lpSrcRect->left, 
                lpSrcRect->bottom - lpSrcRect->top, 
                src_dc, 
                lpSrcRect->left, 
                lpSrcRect->top, 
                SRCCOPY);
                */
        }
        else if (dwFlags & DDBLTFAST_SRCCOLORKEY)
        {
            blt_colorkey(
                dst_buf,
                dst_x,
                dst_y,
                dst_w,
                dst_h,
                This->pitch,
                src_buf,
                src_x,
                src_y,
                src_surface->pitch,
                src_surface->color_key.dwColorSpaceLowValue,
                src_surface->color_key.dwColorSpaceHighValue,
                This->bpp);
        }
        else if (This == src_surface)
        {
            blt_overlap(
                dst_buf,
                dst_x,
                dst_y,
                dst_w,
                dst_h,
                This->pitch,
                src_buf,
                src_x,
                src_y,
                src_surface->pitch,
                This->bpp);
        }
        else
        {
            blt_clean(
                dst_buf,
                dst_x,
                dst_y,
                dst_w,
                dst_h,
                This->pitch,
                src_buf,
                src_x,
                src_y,
                src_surface->pitch,
                This->bpp);
        }
    }

    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && g_ddraw.ref && g_ddraw.render.run)
    {
        InterlockedExchange(&g_ddraw.render.surface_updated, TRUE);

        DWORD time = timeGetTime();

        if (!(This->flags & DDSD_BACKBUFFERCOUNT) ||
            (This->last_flip_tick + FLIP_REDRAW_TIMEOUT < time && This->last_blt_tick + FLIP_REDRAW_TIMEOUT < time))
        {
            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);

            if (g_config.limiter_type == LIMIT_BLTFAST && g_ddraw.ticks_limiter.tick_length > 0)
            {
                g_ddraw.ticks_limiter.dds_unlock_limiter_disabled = TRUE;
                util_limit_game_ticks();
            }
        }
    }

    return DD_OK;
}

HRESULT dds_DeleteAttachedSurface(IDirectDrawSurfaceImpl* This, DWORD dwFlags, IDirectDrawSurfaceImpl* lpDDSurface)
{
    if (lpDDSurface)
    {
        IDirectDrawSurface_Release(lpDDSurface);

        if (lpDDSurface == This->backbuffer)
            This->backbuffer = NULL;
    }

    return DD_OK;
}

HRESULT dds_GetSurfaceDesc(IDirectDrawSurfaceImpl* This, LPDDSURFACEDESC lpDDSurfaceDesc)
{
    if (lpDDSurfaceDesc)
    {
        int size = lpDDSurfaceDesc->dwSize == sizeof(DDSURFACEDESC2) ? sizeof(DDSURFACEDESC2) : sizeof(DDSURFACEDESC);

        memset(lpDDSurfaceDesc, 0, size);

        lpDDSurfaceDesc->dwSize = size;
        lpDDSurfaceDesc->dwFlags = 
            DDSD_CAPS | 
            DDSD_WIDTH | 
            DDSD_HEIGHT | 
            DDSD_PITCH | 
            DDSD_PIXELFORMAT | 
            DDSD_LPSURFACE;

        lpDDSurfaceDesc->dwWidth = This->width;
        lpDDSurfaceDesc->dwHeight = This->height;
        lpDDSurfaceDesc->lPitch = This->pitch;
        lpDDSurfaceDesc->lpSurface = dds_GetBuffer(This);
        lpDDSurfaceDesc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
        lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = This->bpp;
        lpDDSurfaceDesc->ddsCaps.dwCaps = This->caps;

        if (This->flags & DDSD_BACKBUFFERCOUNT)
        {
            lpDDSurfaceDesc->dwFlags |= DDSD_BACKBUFFERCOUNT;
            lpDDSurfaceDesc->dwBackBufferCount = This->backbuffer_count;
        }

        if (This->flags & DDSD_CKSRCBLT)
        {
            lpDDSurfaceDesc->dwFlags |= DDSD_CKSRCBLT;
            lpDDSurfaceDesc->ddckCKSrcBlt.dwColorSpaceHighValue = This->color_key.dwColorSpaceHighValue;
            lpDDSurfaceDesc->ddckCKSrcBlt.dwColorSpaceLowValue = This->color_key.dwColorSpaceLowValue;
        }

        if (This->bpp == 8)
        {
            lpDDSurfaceDesc->ddpfPixelFormat.dwFlags |= DDPF_PALETTEINDEXED8;
        }
        else if (This->bpp == 16)
        {
            lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xF800;
            lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x07E0;
            lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001F;
        }
        else if (This->bpp == 32)
        {
            lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xFF0000;
            lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x00FF00;
            lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x0000FF;
        }
    }

    return DD_OK;
}

HRESULT dds_EnumAttachedSurfaces(
    IDirectDrawSurfaceImpl* This,
    LPVOID lpContext,
    LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback)
{
    static DDSURFACEDESC2 desc;

    memset(&desc, 0, sizeof(desc));

    if (This->backbuffer)
    {
        /* Hack for carmageddon 1 lowres mode */
        if (g_config.carma95_hack && g_ddraw.height == 200)
        {
            dds_GetSurfaceDesc(This, (LPDDSURFACEDESC)&desc);
            lpEnumSurfacesCallback((LPDIRECTDRAWSURFACE)This, (LPDDSURFACEDESC)&desc, lpContext);
        }
        else
        {
            dds_GetSurfaceDesc(This->backbuffer, (LPDDSURFACEDESC)&desc);
            lpEnumSurfacesCallback((LPDIRECTDRAWSURFACE)This->backbuffer, (LPDDSURFACEDESC)&desc, lpContext);
        }
    }

    return DD_OK;
}

HRESULT dds_Flip(IDirectDrawSurfaceImpl* This, IDirectDrawSurfaceImpl* lpDDSurfaceTargetOverride, DWORD dwFlags)
{
    dbg_dump_dds_flip_flags(dwFlags);

    if (This->backbuffer && !This->skip_flip && !(g_config.carma95_hack && g_ddraw.height == 200))
    {
        EnterCriticalSection(&g_ddraw.cs);
        IDirectDrawSurfaceImpl* backbuffer = lpDDSurfaceTargetOverride ? lpDDSurfaceTargetOverride : This->backbuffer;

        void* buf = InterlockedExchangePointer(&This->surface, backbuffer->surface);
        HBITMAP bitmap = (HBITMAP)InterlockedExchangePointer((void*)&This->bitmap, backbuffer->bitmap);
        HDC dc = (HDC)InterlockedExchangePointer((void*)&This->hdc, backbuffer->hdc);
        HANDLE map = (HANDLE)InterlockedExchangePointer(&This->mapping, backbuffer->mapping);

        InterlockedExchangePointer(&backbuffer->surface, buf);
        InterlockedExchangePointer((void*)&backbuffer->bitmap, bitmap);
        InterlockedExchangePointer((void*)&backbuffer->hdc, dc);
        InterlockedExchangePointer(&backbuffer->mapping, map);

        if (g_config.flipclear && (This->caps & DDSCAPS_PRIMARYSURFACE))
        {
            blt_clear(buf, 0, backbuffer->size);
        }

        LeaveCriticalSection(&g_ddraw.cs);

        if (!lpDDSurfaceTargetOverride && This->backbuffer->backbuffer)
        {
            dds_Flip(This->backbuffer, NULL, 0);
        }
    }

    This->skip_flip = FALSE;

    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && g_ddraw.ref && g_ddraw.render.run)
    {
        This->last_flip_tick = timeGetTime();

        InterlockedExchange(&g_ddraw.render.surface_updated, TRUE);
        ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
        SwitchToThread();

        if ((g_config.maxgameticks == 0 && (dwFlags & DDFLIP_WAIT)) || g_config.maxgameticks == -2)
        {
            dd_WaitForVerticalBlank(DDWAITVB_BLOCKEND, NULL);
        }

        if (g_ddraw.ticks_limiter.tick_length > 0)
        {
            g_ddraw.ticks_limiter.dds_unlock_limiter_disabled = TRUE;
            util_limit_game_ticks();
        }
    }

    return DD_OK;
}

HRESULT dds_GetAttachedSurface(IDirectDrawSurfaceImpl* This, LPDDSCAPS lpDdsCaps, IDirectDrawSurfaceImpl** lpDDsurface)
{
    if (!lpDdsCaps || !lpDDsurface)
        return DDERR_INVALIDPARAMS;

    if (This->backbuffer && (This->backbuffer->caps & lpDdsCaps->dwCaps) == lpDdsCaps->dwCaps)
    {
        IDirectDrawSurface_AddRef(This->backbuffer);
        *lpDDsurface = This->backbuffer;

        return DD_OK;
    }

    return DDERR_NOTFOUND;
}

HRESULT dds_GetCaps(IDirectDrawSurfaceImpl* This, LPDDSCAPS lpDDSCaps)
{
    if (!lpDDSCaps)
        return DDERR_INVALIDPARAMS;

    lpDDSCaps->dwCaps = This->caps;

    return DD_OK;
}

HRESULT dds_GetClipper(IDirectDrawSurfaceImpl* This, IDirectDrawClipperImpl** lpClipper)
{
    if (!lpClipper)
        return DDERR_INVALIDPARAMS;

    *lpClipper = This->clipper;

    if (This->clipper)
    {
        IDirectDrawClipper_AddRef(This->clipper);
        return DD_OK;
    }
    else
    {
        return DDERR_NOCLIPPERATTACHED;
    }
}

HRESULT dds_GetColorKey(IDirectDrawSurfaceImpl* This, DWORD dwFlags, LPDDCOLORKEY lpColorKey)
{
    if (dwFlags != DDCKEY_SRCBLT || !lpColorKey)
    {
        TRACE("     NOT_IMPLEMENTED dwFlags=%08X, lpColorKey=%p\n", dwFlags, lpColorKey);
    }

    if (lpColorKey)
    {
        lpColorKey->dwColorSpaceHighValue = This->color_key.dwColorSpaceHighValue;
        lpColorKey->dwColorSpaceLowValue = This->color_key.dwColorSpaceLowValue;
    }

    return DD_OK;
}

HRESULT dds_GetDC(IDirectDrawSurfaceImpl* This, HDC FAR* lpHDC)
{
    if (!This)
    {
        if (lpHDC)
            *lpHDC = NULL;

        return DDERR_INVALIDPARAMS;
    }

    RGBQUAD* data =
        This->palette ? This->palette->data_rgb :
        g_ddraw.ref && g_ddraw.primary && g_ddraw.primary->palette ? g_ddraw.primary->palette->data_rgb :
        NULL;

    HDC dc = This->hdc;

    if (This->backbuffer || (This->caps & DDSCAPS_FLIP))
        dc = (HDC)InterlockedExchangeAdd((LONG*)&This->hdc, 0);

    if (This->bpp == 8 && data)
        SetDIBColorTable(dc, 0, 256, data);

    if (lpHDC)
        *lpHDC = dc;

    return DD_OK;
}

HRESULT dds_GetPalette(IDirectDrawSurfaceImpl* This, IDirectDrawPaletteImpl** lplpDDPalette)
{
    if (!lplpDDPalette)
        return DDERR_INVALIDPARAMS;

    *lplpDDPalette = This->palette;

    if (This->palette)
    {
        IDirectDrawPalette_AddRef(This->palette);
        return DD_OK;
    }
    else
    {
        return DDERR_NOPALETTEATTACHED;
    }
}

HRESULT dds_GetPixelFormat(IDirectDrawSurfaceImpl* This, LPDDPIXELFORMAT ddpfPixelFormat)
{
    if (ddpfPixelFormat)
    {
        memset(ddpfPixelFormat, 0, sizeof(DDPIXELFORMAT));

        ddpfPixelFormat->dwSize = sizeof(DDPIXELFORMAT);
        ddpfPixelFormat->dwFlags = DDPF_RGB;
        ddpfPixelFormat->dwRGBBitCount = This->bpp;

        if (This->bpp == 8)
        {
            ddpfPixelFormat->dwFlags |= DDPF_PALETTEINDEXED8;
        }
        else if (This->bpp == 16)
        {
            ddpfPixelFormat->dwRBitMask = 0xF800;
            ddpfPixelFormat->dwGBitMask = 0x07E0;
            ddpfPixelFormat->dwBBitMask = 0x001F;
        }
        else if (This->bpp == 32)
        {
            ddpfPixelFormat->dwRBitMask = 0xFF0000;
            ddpfPixelFormat->dwGBitMask = 0x00FF00;
            ddpfPixelFormat->dwBBitMask = 0x0000FF;
        }

        return DD_OK;
    }

    return DDERR_INVALIDPARAMS;
}

HRESULT dds_Lock(
    IDirectDrawSurfaceImpl* This,
    LPRECT lpDestRect,
    LPDDSURFACEDESC lpDDSurfaceDesc,
    DWORD dwFlags,
    HANDLE hEvent)
{
    if (g_config.lock_surfaces)
        EnterCriticalSection(&This->cs);

    dbg_dump_dds_lock_flags(dwFlags);

    if (g_ddraw.ref && g_config.fixnotresponding && !IsWine())
    {
        MSG msg; /* workaround for "Not Responding" window problem */
        real_PeekMessageA(&msg, g_ddraw.hwnd, 0, 0, PM_NOREMOVE);
    }

    HRESULT ret = dds_GetSurfaceDesc(This, lpDDSurfaceDesc);

    if (lpDestRect && lpDDSurfaceDesc)
    {
        if (lpDestRect->left < 0 ||
            lpDestRect->top < 0 ||
            lpDestRect->left > lpDestRect->right ||
            lpDestRect->top > lpDestRect->bottom ||
            lpDestRect->right > This->width ||
            lpDestRect->bottom > This->height)
        {
            lpDDSurfaceDesc->lpSurface = NULL;

            return DDERR_INVALIDPARAMS;
        }

        lpDDSurfaceDesc->lpSurface =
            (char*)dds_GetBuffer(This) + (lpDestRect->left * This->bytes_pp) + (lpDestRect->top * This->pitch);
    }

    return ret;
}

HRESULT dds_ReleaseDC(IDirectDrawSurfaceImpl* This, HDC hDC)
{
    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && g_ddraw.ref && g_ddraw.render.run)
    {
        InterlockedExchange(&g_ddraw.render.surface_updated, TRUE);

        DWORD time = timeGetTime();

        if (!(This->flags & DDSD_BACKBUFFERCOUNT) ||
            (This->last_flip_tick + FLIP_REDRAW_TIMEOUT < time && This->last_blt_tick + FLIP_REDRAW_TIMEOUT < time))
        {
            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
        }
    }

    return DD_OK;
}

HRESULT dds_SetClipper(IDirectDrawSurfaceImpl* This, IDirectDrawClipperImpl* lpClipper)
{
    if (lpClipper)
    {
        IDirectDrawClipper_AddRef(lpClipper);

        if ((This->caps & DDSCAPS_PRIMARYSURFACE) && lpClipper->hwnd)
        {
            RECT rc = { 0, 0, This->width, This->height };
            ddc_SetClipRect(lpClipper, &rc);
        }
    }

    if (This->clipper)
        IDirectDrawClipper_Release(This->clipper);

    This->clipper = lpClipper;

    return DD_OK;
}

HRESULT dds_SetColorKey(IDirectDrawSurfaceImpl* This, DWORD dwFlags, LPDDCOLORKEY lpColorKey)
{
    if (dwFlags != DDCKEY_SRCBLT || !lpColorKey)
    {
        TRACE("     NOT_IMPLEMENTED dwFlags=%08X, lpColorKey=%p\n", dwFlags, lpColorKey);
    }

    if (lpColorKey)
    {
        This->flags |= DDSD_CKSRCBLT;

        This->color_key.dwColorSpaceLowValue = lpColorKey->dwColorSpaceLowValue;

        if (dwFlags & DDCKEY_COLORSPACE)
        {
            This->color_key.dwColorSpaceHighValue = lpColorKey->dwColorSpaceHighValue;
        }
        else
        {
            This->color_key.dwColorSpaceHighValue = lpColorKey->dwColorSpaceLowValue;
        }
    }

    return DD_OK;
}

HRESULT dds_SetPalette(IDirectDrawSurfaceImpl* This, IDirectDrawPaletteImpl* lpDDPalette)
{
    if (lpDDPalette)
        IDirectDrawPalette_AddRef(lpDDPalette);

    if (This->palette)
        IDirectDrawPalette_Release(This->palette);

    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && g_ddraw.ref)
    {
        EnterCriticalSection(&g_ddraw.cs);
        This->palette = lpDDPalette;
        LeaveCriticalSection(&g_ddraw.cs);

        if (g_ddraw.render.run)
        {
            InterlockedExchange(&g_ddraw.render.palette_updated, TRUE);
            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);
        }
    }
    else
    {
        This->palette = lpDDPalette;
    }

    return DD_OK;
}

HRESULT dds_Unlock(IDirectDrawSurfaceImpl* This, LPRECT lpRect)
{
    /* Hack for Warcraft II BNE and Diablo */
    HWND hwnd = g_ddraw.ref && g_ddraw.bnet_active ? FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL) : NULL;

    if (hwnd && (This->caps & DDSCAPS_PRIMARYSURFACE))
    {
        HDC primary_dc;
        dds_GetDC(This, &primary_dc);

        /* GdiTransparentBlt idea taken from Aqrit's war2 ddraw */

        RGBQUAD quad;
        GetDIBColorTable(primary_dc, 0xFE, 1, &quad);
        COLORREF color = RGB(quad.rgbRed, quad.rgbGreen, quad.rgbBlue);
        BOOL erase = FALSE;

        do
        {
            RECT rc;
            if (fake_GetWindowRect(hwnd, &rc))
            {
                if (rc.bottom - rc.top == 479)
                    erase = TRUE;

                HDC hdc = GetDCEx(hwnd, NULL, DCX_PARENTCLIP | DCX_CACHE);

                GdiTransparentBlt(
                    hdc,
                    0,
                    0,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    primary_dc,
                    rc.left,
                    rc.top,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    color
                );

                ReleaseDC(hwnd, hdc);
            }

        } while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, "SDlgDialog", NULL)));

        if (erase)
        {
            blt_clear(This->surface, 0xFE, This->size);
        }
    }

    /* Hack for Star Trek Armada */
    hwnd = g_ddraw.ref && g_config.armadahack ? FindWindowEx(HWND_DESKTOP, NULL, "#32770", NULL) : NULL;

    if (hwnd && (This->caps & DDSCAPS_PRIMARYSURFACE))
    {
        HDC primary_dc;
        dds_GetDC(This, &primary_dc);

        RECT rc;
        if (fake_GetWindowRect(hwnd, &rc))
        {
            HDC hdc = GetDC(hwnd);

            GdiTransparentBlt(
                hdc,
                0,
                0,
                rc.right - rc.left,
                rc.bottom - rc.top,
                primary_dc,
                rc.left,
                rc.top,
                rc.right - rc.left,
                rc.bottom - rc.top,
                0
            );

            ReleaseDC(hwnd, hdc);
        }

        blt_clear(This->surface, 0x00, This->size);
    }


    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && g_ddraw.ref && g_ddraw.render.run)
    {
        InterlockedExchange(&g_ddraw.render.surface_updated, TRUE);

        DWORD time = timeGetTime();

        if (!(This->flags & DDSD_BACKBUFFERCOUNT) ||
            (This->last_flip_tick + FLIP_REDRAW_TIMEOUT < time && This->last_blt_tick + FLIP_REDRAW_TIMEOUT < time))
        {
            ReleaseSemaphore(g_ddraw.render.sem, 1, NULL);

            if (g_ddraw.ticks_limiter.tick_length > 0 && !g_ddraw.ticks_limiter.dds_unlock_limiter_disabled)
                util_limit_game_ticks();
        }
    }

    if (g_config.lock_surfaces)
        LeaveCriticalSection(&This->cs);

    return DD_OK;
}

HRESULT dds_GetDDInterface(IDirectDrawSurfaceImpl* This, LPVOID* lplpDD)
{
    if (!lplpDD)
        return DDERR_INVALIDPARAMS;

    *lplpDD = This->ddraw;
    IDirectDraw_AddRef(This->ddraw);

    return DD_OK;
}

HRESULT dds_SetSurfaceDesc(IDirectDrawSurfaceImpl* This, LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags)
{
    dbg_dump_dds_flags(lpDDSD->dwFlags);
    dbg_dump_dds_caps(lpDDSD->ddsCaps.dwCaps);

    if ((lpDDSD->dwFlags & DDSD_LPSURFACE) == 0)
        return DDERR_UNSUPPORTED;


    if (This->bitmap)
    {
        DeleteObject(This->bitmap);
        InterlockedDecrement(&g_dds_gdi_handles);
        This->bitmap = NULL;
    }
    else if (This->surface && !This->custom_buf)
    {
        HeapFree(GetProcessHeap(), 0, This->surface);
        This->surface = NULL;
    }

    if (This->hdc)
    {
        DeleteDC(This->hdc);
        InterlockedDecrement(&g_dds_gdi_handles);
        This->hdc = NULL;
    }

    if (This->bmi)
    {
        HeapFree(GetProcessHeap(), 0, This->bmi);
        This->bmi = NULL;
    }

    if (This->mapping)
    {
        CloseHandle(This->mapping);
        This->mapping = NULL;
    }

    if (lpDDSD->dwFlags & DDSD_PIXELFORMAT)
    {
        switch (lpDDSD->ddpfPixelFormat.dwRGBBitCount)
        {
        case 0:
            break;
        case 8:
            This->bpp = 8;
            break;
        case 15:
            TRACE("     NOT_IMPLEMENTED bpp=%u\n", lpDDSD->ddpfPixelFormat.dwRGBBitCount);
        case 16:
            This->bpp = 16;
            break;
        case 24:
            TRACE("     NOT_IMPLEMENTED bpp=%u\n", lpDDSD->ddpfPixelFormat.dwRGBBitCount);
        case 32:
            This->bpp = 32;
            break;
        default:
            TRACE("     NOT_IMPLEMENTED bpp=%u\n", lpDDSD->ddpfPixelFormat.dwRGBBitCount);
            break;
        }
    }

    if (lpDDSD->dwFlags & DDSD_WIDTH)
    {
        This->width = lpDDSD->dwWidth;
    }

    if (lpDDSD->dwFlags & DDSD_HEIGHT)
    {
        This->height = lpDDSD->dwHeight;
    }

    if (lpDDSD->dwFlags & DDSD_PITCH)
    {
        This->pitch = lpDDSD->lPitch;
    }

    if (lpDDSD->dwFlags & DDSD_LPSURFACE)
    {
        This->surface = lpDDSD->lpSurface;
    }

    This->bytes_pp = This->bpp / 8;
    This->size = This->pitch * This->height;
    This->custom_buf = TRUE;

    return DD_OK;
}

void* dds_GetBuffer(IDirectDrawSurfaceImpl* This)
{
    if (!This)
        return NULL;

    if (This->backbuffer || (This->caps & DDSCAPS_FLIP))
        return (void*)InterlockedExchangeAdd((LONG*)&This->surface, 0);

    return This->surface;
}

HRESULT dd_CreateSurface(
    IDirectDrawImpl* This,
    LPDDSURFACEDESC lpDDSurfaceDesc,
    IDirectDrawSurfaceImpl** lpDDSurface,
    IUnknown FAR* unkOuter)
{
    dbg_dump_dds_flags(lpDDSurfaceDesc->dwFlags);
    dbg_dump_dds_caps(lpDDSurfaceDesc->ddsCaps.dwCaps);

    if (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_OVERLAY)
        return DDERR_UNSUPPORTED;

    if (!(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
        (lpDDSurfaceDesc->dwWidth > 16384 || lpDDSurfaceDesc->dwHeight > 16384))
    {
        return DDERR_INVALIDPARAMS;
    }

    if ((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
        g_ddraw.primary &&
        g_ddraw.primary->width == g_ddraw.width &&
        g_ddraw.primary->height == g_ddraw.height &&
        g_ddraw.primary->bpp == g_ddraw.bpp)
    {
        g_ddraw.primary->skip_flip = TRUE;

        *lpDDSurface = g_ddraw.primary;
        IDirectDrawSurface_AddRef(g_ddraw.primary);

        return DD_OK;
    }

    IDirectDrawSurfaceImpl* dst_surface =
        (IDirectDrawSurfaceImpl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawSurfaceImpl));

    dst_surface->lpVtbl = &g_dds_vtbl;

    lpDDSurfaceDesc->dwFlags |= DDSD_CAPS;

    InitializeCriticalSection(&dst_surface->cs);

    dst_surface->bpp = g_ddraw.bpp == 0 ? 16 : g_ddraw.bpp;
    dst_surface->flags = lpDDSurfaceDesc->dwFlags;
    dst_surface->caps = lpDDSurfaceDesc->ddsCaps.dwCaps;
    dst_surface->ddraw = This;

    if (dst_surface->flags & DDSD_CKSRCBLT)
    {
        dst_surface->color_key.dwColorSpaceHighValue = lpDDSurfaceDesc->ddckCKSrcBlt.dwColorSpaceHighValue;
        dst_surface->color_key.dwColorSpaceLowValue = lpDDSurfaceDesc->ddckCKSrcBlt.dwColorSpaceLowValue;
    }

    if (dst_surface->flags & DDSD_PIXELFORMAT)
    {
        switch (lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount)
        {
        case 8:
            dst_surface->bpp = 8;
            break;
        case 15:
            TRACE("     NOT_IMPLEMENTED bpp=%u\n", lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount);
        case 16:
            dst_surface->bpp = 16;
            break;
        case 24:
            TRACE("     NOT_IMPLEMENTED bpp=%u\n", lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount);
        case 32:
            dst_surface->bpp = 32;
            break;
        default:
            TRACE("     NOT_IMPLEMENTED bpp=%u\n", lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount);
            break;
        }
    }

    if (dst_surface->caps & DDSCAPS_PRIMARYSURFACE)
    {
        if (dst_surface->caps & DDSCAPS_FLIP)
        {
            dst_surface->caps |= DDSCAPS_FRONTBUFFER;
        }

        if (!(dst_surface->caps & DDSCAPS_SYSTEMMEMORY))
        {
            dst_surface->caps |= DDSCAPS_VIDEOMEMORY;
        }

        dst_surface->caps |= DDSCAPS_VISIBLE;

        dst_surface->width = g_ddraw.width;
        dst_surface->height = g_ddraw.height;
    }
    else
    {
        if (!(dst_surface->caps & DDSCAPS_SYSTEMMEMORY) || g_config.tshack)
        {
            dst_surface->caps |= DDSCAPS_VIDEOMEMORY;
        }

        dst_surface->width = lpDDSurfaceDesc->dwWidth;
        dst_surface->height = lpDDSurfaceDesc->dwHeight;
    }

    if ((dst_surface->flags & DDSD_LPSURFACE) && (dst_surface->flags & DDSD_PITCH))
    {
        dst_surface->surface = lpDDSurfaceDesc->lpSurface;
        dst_surface->pitch = lpDDSurfaceDesc->lPitch;
        dst_surface->bytes_pp = dst_surface->bpp / 8;
        dst_surface->size = dst_surface->pitch * dst_surface->height;
        dst_surface->custom_buf = TRUE;
    }
    else if (dst_surface->width && dst_surface->height)
    {
        dst_surface->bytes_pp = dst_surface->bpp / 8;
        dst_surface->pitch = ((dst_surface->width * dst_surface->bpp + 63) & ~63) >> 3;
        dst_surface->size = dst_surface->pitch * dst_surface->height;

        DWORD aligned_width = dst_surface->pitch / dst_surface->bytes_pp;

        DWORD bmi_size = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256;
        DWORD bmp_size = dst_surface->pitch * (dst_surface->height + g_config.guard_lines);

        dst_surface->bmi = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bmi_size);
        dst_surface->bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        dst_surface->bmi->bmiHeader.biWidth = aligned_width;
        dst_surface->bmi->bmiHeader.biHeight = -((int)dst_surface->height + g_config.guard_lines);
        dst_surface->bmi->bmiHeader.biPlanes = 1;
        dst_surface->bmi->bmiHeader.biBitCount = dst_surface->bpp;
        dst_surface->bmi->bmiHeader.biCompression = dst_surface->bpp == 8 ? BI_RGB : BI_BITFIELDS;

        WORD clr_bits = (WORD)(dst_surface->bmi->bmiHeader.biPlanes * dst_surface->bmi->bmiHeader.biBitCount);

        if (clr_bits < 24)
        {
            dst_surface->bmi->bmiHeader.biClrUsed = (1 << clr_bits);
        }

        dst_surface->bmi->bmiHeader.biSizeImage =
            ((aligned_width * clr_bits + 63) & ~63) / 8 * dst_surface->height;

        if (dst_surface->bpp == 8)
        {
            for (int i = 0; i < 256; i++)
            {
                dst_surface->bmi->bmiColors[i].rgbRed = i;
                dst_surface->bmi->bmiColors[i].rgbGreen = i;
                dst_surface->bmi->bmiColors[i].rgbBlue = i;
                dst_surface->bmi->bmiColors[i].rgbReserved = 0;
            }
        }
        else if (dst_surface->bpp == 16 && g_config.rgb555)
        {
            ((DWORD*)dst_surface->bmi->bmiColors)[0] = 0x7C00;
            ((DWORD*)dst_surface->bmi->bmiColors)[1] = 0x03E0;
            ((DWORD*)dst_surface->bmi->bmiColors)[2] = 0x001F;
        }
        else if (dst_surface->bpp == 16)
        {
            ((DWORD*)dst_surface->bmi->bmiColors)[0] = 0xF800;
            ((DWORD*)dst_surface->bmi->bmiColors)[1] = 0x07E0;
            ((DWORD*)dst_surface->bmi->bmiColors)[2] = 0x001F;
        }
        else if (dst_surface->bpp == 32)
        {
            ((DWORD*)dst_surface->bmi->bmiColors)[0] = 0xFF0000;
            ((DWORD*)dst_surface->bmi->bmiColors)[1] = 0x00FF00;
            ((DWORD*)dst_surface->bmi->bmiColors)[2] = 0x0000FF;
        }

        /* Claw hack: 128x128 surfaces need a DC for custom levels to work properly */
        if (InterlockedExchangeAdd(&g_dds_gdi_handles, 0) < 4000 || 
            (dst_surface->width == g_ddraw.width && dst_surface->height == g_ddraw.height) ||
            (dst_surface->width == 128 && dst_surface->height == 128))
        {
            dst_surface->hdc = CreateCompatibleDC(g_ddraw.render.hdc);

            if (dst_surface->hdc)
                InterlockedIncrement(&g_dds_gdi_handles);

            dst_surface->mapping =
                CreateFileMappingA(
                    INVALID_HANDLE_VALUE,
                    NULL,
                    PAGE_READWRITE | SEC_COMMIT,
                    0,
                    bmp_size + 256,
                    NULL);

            DWORD map_offset = 0;

            if (dst_surface->mapping)
            {
                LPVOID data = MapViewOfFile(dst_surface->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
                if (data)
                {
                    while (((DWORD)data + map_offset) % 128) map_offset++;
                    UnmapViewOfFile(data);
                }

                if (!data || (map_offset % sizeof(DWORD)))
                {
                    map_offset = 0;
                    CloseHandle(dst_surface->mapping);
                    dst_surface->mapping = NULL;
                }
            }

            dst_surface->bitmap =
                CreateDIBSection(
                    dst_surface->hdc,
                    dst_surface->bmi,
                    DIB_RGB_COLORS,
                    (void**)&dst_surface->surface,
                    dst_surface->mapping,
                    map_offset);

            if (dst_surface->bitmap)
                InterlockedIncrement(&g_dds_gdi_handles);
        }

        dst_surface->bmi->bmiHeader.biHeight = -((int)dst_surface->height);

        if (!dst_surface->bitmap)
        {
            dst_surface->surface = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bmp_size);
        }
        else
        {
            SelectObject(dst_surface->hdc, dst_surface->bitmap);
        }

        if (dst_surface->caps & DDSCAPS_PRIMARYSURFACE)
        {
            g_ddraw.primary = dst_surface;
            FakePrimarySurface = dst_surface->surface;
        }
    }

    if (dst_surface->flags & DDSD_BACKBUFFERCOUNT)
    {
        dst_surface->backbuffer_count = lpDDSurfaceDesc->dwBackBufferCount;

        TRACE("     dwBackBufferCount=%d\n", lpDDSurfaceDesc->dwBackBufferCount);

        DDSURFACEDESC desc;
        memset(&desc, 0, sizeof(desc));

        desc.dwFlags |= DDSD_CAPS;

        if (lpDDSurfaceDesc->dwBackBufferCount > 1)
        {
            desc.dwBackBufferCount = lpDDSurfaceDesc->dwBackBufferCount - 1;
            desc.dwFlags |= DDSD_BACKBUFFERCOUNT;
        }

        if (dst_surface->caps & DDSCAPS_FRONTBUFFER)
        {
            desc.ddsCaps.dwCaps |= DDSCAPS_BACKBUFFER;
        }

        if (dst_surface->caps & DDSCAPS_FLIP)
        {
            desc.ddsCaps.dwCaps |= DDSCAPS_FLIP;
        }

        if (dst_surface->caps & DDSCAPS_COMPLEX)
        {
            desc.ddsCaps.dwCaps |= DDSCAPS_COMPLEX;
        }

        if (dst_surface->caps & DDSCAPS_VIDEOMEMORY)
        {
            desc.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
        }

        if (dst_surface->caps & DDSCAPS_SYSTEMMEMORY)
        {
            desc.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
        }

        desc.dwWidth = dst_surface->width;
        desc.dwHeight = dst_surface->height;

        dd_CreateSurface(This, &desc, &dst_surface->backbuffer, unkOuter);
    }

    TRACE(
        "     surface = %p (%ux%u@%u), buf = %p\n",
        dst_surface,
        dst_surface->width,
        dst_surface->height,
        dst_surface->bpp,
        dst_surface->surface);

    *lpDDSurface = dst_surface;

    dst_surface->ref = 0;
    IDirectDrawSurface_AddRef(dst_surface);

    return DD_OK;
}
