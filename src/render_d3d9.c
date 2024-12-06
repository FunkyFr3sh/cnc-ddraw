#include <windows.h>
#include <stdio.h>
#include <d3d9.h>
#include "fps_limiter.h"
#include "dd.h"
#include "ddsurface.h"
#include "d3d9shader.h"
#include "render_d3d9.h"
#include "utils.h"
#include "wndproc.h"
#include "blt.h"
#include "debug.h"
#include "d3d9types.h"
#include "hook.h"
#include "config.h"


static BOOL d3d9_create_resources();
static BOOL d3d9_set_states();
static BOOL d3d9_update_vertices(BOOL upscale_hack, BOOL stretch);

static D3D9RENDERER g_d3d9;

BOOL d3d9_is_available()
{
    LPDIRECT3D9 d3d9 = NULL;

    if ((g_d3d9.hmodule = real_LoadLibraryA("d3d9.dll")))
    {
        IDirect3D9* (WINAPI * d3d_create9)(UINT) =
            (IDirect3D9 * (WINAPI*)(UINT))real_GetProcAddress(g_d3d9.hmodule, "Direct3DCreate9");

        if (d3d_create9 && (d3d9 = d3d_create9(D3D_SDK_VERSION)))
            IDirect3D9_Release(d3d9);
    }

    return d3d9 != NULL;
}

BOOL d3d9_create()
{
    if (g_d3d9.hwnd == g_ddraw->hwnd && d3d9_create_resources() && d3d9_reset(g_config.windowed))
    {
        return TRUE;
    }
    
    d3d9_release();

    if (!g_d3d9.hmodule)
        g_d3d9.hmodule = real_LoadLibraryA("d3d9.dll");

    if (g_d3d9.hmodule)
    {
        LPDIRECT3D9 d3d9on12 = NULL;
        D3D9ON12_ARGS args;
        memset(&args, 0, sizeof(args));
        args.Enable9On12 = TRUE;

        IDirect3D9* (WINAPI * d3d_create9on12)(INT, D3D9ON12_ARGS*, UINT) = NULL;
        IDirect3D9* (WINAPI * d3d_create9)(UINT) = (void*)real_GetProcAddress(g_d3d9.hmodule, "Direct3DCreate9");

        if (g_ddraw->d3d9on12)
        {
            d3d_create9on12 = (void*)real_GetProcAddress(g_d3d9.hmodule, "Direct3DCreate9On12");
        }

        if ((d3d_create9on12 && (d3d9on12 = g_d3d9.instance = d3d_create9on12(D3D_SDK_VERSION, &args, 1))) ||
            (d3d_create9 && (g_d3d9.instance = d3d_create9(D3D_SDK_VERSION))))
        {
#if _DEBUG 
            D3DADAPTER_IDENTIFIER9 ai = {0};
            D3DCAPS9 caps = { 0 };
            HRESULT hr = IDirect3D9_GetAdapterIdentifier(g_d3d9.instance, D3DADAPTER_DEFAULT, 0, &ai);
            HRESULT hr2 = IDirect3D9_GetDeviceCaps(g_d3d9.instance, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);

            if (SUCCEEDED(hr)) 
            {
                TRACE("+--Direct3D9-------------------------------------\n");
                TRACE("| D3D9On12:            %s\n", d3d9on12 != NULL ? "True" : "False");
                TRACE("| VendorId:            0x%x\n", ai.VendorId);
                TRACE("| DeviceId:            0x%x\n", ai.DeviceId);
                TRACE("| Revision:            0x%x\n", ai.Revision);
                TRACE("| SubSysId:            0x%x\n", ai.SubSysId);
                TRACE("| Version:             %hu.%hu.%hu.%hu\n", 
                    HIWORD(ai.DriverVersion.HighPart), 
                    LOWORD(ai.DriverVersion.HighPart), 
                    HIWORD(ai.DriverVersion.LowPart), 
                    LOWORD(ai.DriverVersion.LowPart));

                TRACE("| Driver:              %s\n", ai.Driver);
                TRACE("| Description:         %s\n", ai.Description);

                if (SUCCEEDED(hr2))
                {
                    TRACE("| MaxTextureWidth:     %d\n", caps.MaxTextureWidth);
                    TRACE("| MaxTextureHeight:    %d\n", caps.MaxTextureHeight);

                    TRACE("| VertexShaderVersion: %d.%d\n",
                        (caps.VertexShaderVersion >> 8) & 0xFF,
                        caps.VertexShaderVersion & 0xFF);

                    TRACE("| PixelShaderVersion:  %d.%d\n",
                        (caps.PixelShaderVersion >> 8) & 0xFF,
                        caps.PixelShaderVersion & 0xFF);
                }
                TRACE("+------------------------------------------------\n");
            }
#endif
            g_d3d9.hwnd = g_ddraw->hwnd;

            memset(&g_d3d9.params, 0, sizeof(g_d3d9.params));

            g_d3d9.params.Windowed = g_config.windowed || g_config.nonexclusive;
            g_d3d9.params.SwapEffect = D3DSWAPEFFECT_DISCARD;
            g_d3d9.params.hDeviceWindow = g_ddraw->hwnd;
            g_d3d9.params.PresentationInterval = g_config.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
            g_d3d9.params.BackBufferWidth = g_d3d9.params.Windowed ? 0 : g_ddraw->render.width;
            g_d3d9.params.BackBufferHeight = g_d3d9.params.Windowed ? 0 : g_ddraw->render.height;
            g_d3d9.params.FullScreen_RefreshRateInHz = g_d3d9.params.Windowed ? 0 : g_config.refresh_rate;
            g_d3d9.params.BackBufferFormat = g_ddraw->mode.dmBitsPerPel == 16 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
            g_d3d9.params.BackBufferCount = 1;

            DWORD behavior_flags[] = {
                D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
                D3DCREATE_MULTITHREADED | D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
            };

            for (int i = 0; i < sizeof(behavior_flags) / sizeof(behavior_flags[0]); i++)
            {
                if (SUCCEEDED(
                    IDirect3D9_CreateDevice(
                        g_d3d9.instance,
                        D3DADAPTER_DEFAULT,
                        D3DDEVTYPE_HAL,
                        g_ddraw->hwnd,
                        behavior_flags[i],
                        &g_d3d9.params,
                        &g_d3d9.device)))
                    return g_d3d9.device && d3d9_create_resources() && d3d9_set_states();
            }
        }
    }

    return FALSE;
}

BOOL d3d9_on_device_lost()
{
    if (g_d3d9.device && IDirect3DDevice9_TestCooperativeLevel(g_d3d9.device) == D3DERR_DEVICENOTRESET)
    {
        return d3d9_reset(g_config.windowed);
    }

    return FALSE;
}

BOOL d3d9_reset(BOOL windowed)
{
    g_d3d9.params.Windowed = windowed || g_config.nonexclusive;
    g_d3d9.params.BackBufferWidth = g_d3d9.params.Windowed ? 0 : g_ddraw->render.width;
    g_d3d9.params.BackBufferHeight = g_d3d9.params.Windowed ? 0 : g_ddraw->render.height;
    g_d3d9.params.FullScreen_RefreshRateInHz = g_d3d9.params.Windowed ? 0 : g_config.refresh_rate;
    g_d3d9.params.BackBufferFormat = g_ddraw->mode.dmBitsPerPel == 16 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;

    if (g_d3d9.device && SUCCEEDED(IDirect3DDevice9_Reset(g_d3d9.device, &g_d3d9.params)))
    {
        BOOL result = d3d9_set_states();

        if (result)
        {
            InterlockedExchange(&g_ddraw->render.palette_updated, TRUE);
            InterlockedExchange(&g_ddraw->render.surface_updated, TRUE);
            ReleaseSemaphore(g_ddraw->render.sem, 1, NULL);
        }

        return result;
    }

    return FALSE;
}

BOOL d3d9_release_resources()
{
    if (g_d3d9.pixel_shader)
    {
        IDirect3DPixelShader9_Release(g_d3d9.pixel_shader);
        g_d3d9.pixel_shader = NULL;
    }

    if (g_d3d9.pixel_shader_upscale)
    {
        IDirect3DPixelShader9_Release(g_d3d9.pixel_shader_upscale);
        g_d3d9.pixel_shader_upscale = NULL;
    }

    for (int i = 0; i < D3D9_TEXTURE_COUNT; i++)
    {
        if (g_d3d9.surface_tex[i])
        {
            IDirect3DTexture9_Release(g_d3d9.surface_tex[i]);
            g_d3d9.surface_tex[i] = NULL;
        }

        if (g_d3d9.palette_tex[i])
        {
            IDirect3DTexture9_Release(g_d3d9.palette_tex[i]);
            g_d3d9.palette_tex[i] = NULL;
        }
    }

    if (g_d3d9.vertex_buf)
    {
        IDirect3DVertexBuffer9_Release(g_d3d9.vertex_buf);
        g_d3d9.vertex_buf = NULL;
    }

    return TRUE;
}

BOOL d3d9_release()
{
    d3d9_release_resources();

    if (g_d3d9.device)
    {
        while (IDirect3DDevice9_Release(g_d3d9.device));
        g_d3d9.device = NULL;
    }

    if (g_d3d9.instance)
    {
        while (IDirect3D9_Release(g_d3d9.instance));
        g_d3d9.instance = NULL;
    }

    return TRUE;
}

static BOOL d3d9_create_resources()
{
    if (!g_d3d9.device)
        return FALSE;

    d3d9_release_resources();

    BOOL err = FALSE;

    int width = g_ddraw->width;
    int height = g_ddraw->height;

    g_d3d9.tex_width =
        width <= 1024 ? 1024 : width <= 2048 ? 2048 : width <= 4096 ? 4096 : width;

    g_d3d9.tex_height =
        height <= g_d3d9.tex_width ? g_d3d9.tex_width : height <= 2048 ? 2048 : height <= 4096 ? 4096 : height;

    g_d3d9.tex_width = g_d3d9.tex_width > g_d3d9.tex_height ? g_d3d9.tex_width : g_d3d9.tex_height;

    g_d3d9.scale_w = (float)width / g_d3d9.tex_width;;
    g_d3d9.scale_h = (float)height / g_d3d9.tex_height;

    err = err || FAILED(
        IDirect3DDevice9_CreateVertexBuffer(
            g_d3d9.device,
            sizeof(CUSTOMVERTEX) * 4, 0,
            D3DFVF_XYZRHW | D3DFVF_TEX1,
            D3DPOOL_MANAGED,
            &g_d3d9.vertex_buf,
            NULL));

    err = err || !d3d9_update_vertices(InterlockedExchangeAdd(&g_ddraw->upscale_hack_active, 0), TRUE);

    for (int i = 0; i < D3D9_TEXTURE_COUNT; i++)
    {
        err = err || FAILED(
            IDirect3DDevice9_CreateTexture(
                g_d3d9.device,
                g_d3d9.tex_width,
                g_d3d9.tex_height,
                1,
                0,
                g_ddraw->bpp == 16 ? (g_config.rgb555 ? D3DFMT_X1R5G5B5 : D3DFMT_R5G6B5) : g_ddraw->bpp == 32 ? D3DFMT_X8R8G8B8 : D3DFMT_L8,
                D3DPOOL_MANAGED,
                &g_d3d9.surface_tex[i],
                0));

        err = err || !g_d3d9.surface_tex[i];

        if (g_ddraw->bpp == 8)
        {
            err = err || FAILED(
                IDirect3DDevice9_CreateTexture(
                    g_d3d9.device,
                    256,
                    256,
                    1,
                    0,
                    D3DFMT_X8R8G8B8,
                    D3DPOOL_MANAGED,
                    &g_d3d9.palette_tex[i],
                    0));

            err = err || !g_d3d9.palette_tex[i];
        }
    }

    if (g_ddraw->bpp == 8)
    {
        err = err || FAILED(
            IDirect3DDevice9_CreatePixelShader(g_d3d9.device, (DWORD*)D3D9_PALETTE_SHADER, &g_d3d9.pixel_shader));

        IDirect3DDevice9_CreatePixelShader(
            g_d3d9.device, 
            (DWORD*)D3D9_PALETTE_SHADER_BILINEAR, 
            &g_d3d9.pixel_shader_upscale);
    }
    else
    {
        if (g_config.d3d9_filter == FILTER_LANCZOS)
        {
            BOOL error = FAILED(
                IDirect3DDevice9_CreatePixelShader(
                    g_d3d9.device,
                    (DWORD*)D3D9_LANCZOS2_SHADER,
                    &g_d3d9.pixel_shader_upscale));

            if (error || !g_d3d9.pixel_shader_upscale)
            {
                g_config.d3d9_filter = FILTER_CUBIC;
            }
        }

        if (g_config.d3d9_filter == FILTER_CUBIC)
        {
            IDirect3DDevice9_CreatePixelShader(
                g_d3d9.device,
                (DWORD*)D3D9_CATMULL_ROM_SHADER,
                &g_d3d9.pixel_shader_upscale);
        }
    }

    return g_d3d9.vertex_buf && (g_d3d9.pixel_shader || g_ddraw->bpp == 16 || g_ddraw->bpp == 32) && !err;
}

static BOOL d3d9_set_states()
{
    BOOL err = FALSE;

    err = err || FAILED(IDirect3DDevice9_SetFVF(g_d3d9.device, D3DFVF_XYZRHW | D3DFVF_TEX1));
    err = err || FAILED(IDirect3DDevice9_SetStreamSource(g_d3d9.device, 0, g_d3d9.vertex_buf, 0, sizeof(CUSTOMVERTEX)));
    err = err || FAILED(IDirect3DDevice9_SetTexture(g_d3d9.device, 0, (IDirect3DBaseTexture9*)g_d3d9.surface_tex[0]));

    if (g_ddraw->bpp == 8)
    {
        err = err || FAILED(IDirect3DDevice9_SetTexture(g_d3d9.device, 1, (IDirect3DBaseTexture9*)g_d3d9.palette_tex[0]));
        
        BOOL bilinear =
            g_config.d3d9_filter &&
            g_d3d9.pixel_shader_upscale &&
            (g_ddraw->render.viewport.width != g_ddraw->width || g_ddraw->render.viewport.height != g_ddraw->height);

        err = err || FAILED(
            IDirect3DDevice9_SetPixelShader(
                g_d3d9.device, 
                bilinear ? g_d3d9.pixel_shader_upscale : g_d3d9.pixel_shader));

        if (bilinear)
        {
            float texture_size[4] = { (float)g_d3d9.tex_width, (float)g_d3d9.tex_height, 0, 0 };
            err = err || FAILED(IDirect3DDevice9_SetPixelShaderConstantF(g_d3d9.device, 0, texture_size, 1));
        }
    }
    else
    {
        if (g_config.d3d9_filter)
        {
            if (g_config.d3d9_filter == FILTER_LANCZOS &&
                g_d3d9.pixel_shader_upscale &&
                (g_ddraw->render.viewport.width != g_ddraw->width || 
                    g_ddraw->render.viewport.height != g_ddraw->height) &&
                SUCCEEDED(IDirect3DDevice9_SetPixelShader(g_d3d9.device, g_d3d9.pixel_shader_upscale)))
            {
                float texture_size[4] = { (float)g_d3d9.tex_width, (float)g_d3d9.tex_height, 0, 0 };
                err = err || FAILED(IDirect3DDevice9_SetPixelShaderConstantF(g_d3d9.device, 0, texture_size, 1));
            }
            else if (
                SUCCEEDED(IDirect3DDevice9_SetSamplerState(g_d3d9.device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)) &&
                SUCCEEDED(IDirect3DDevice9_SetSamplerState(g_d3d9.device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)) &&
                g_config.d3d9_filter == FILTER_CUBIC &&
                g_d3d9.pixel_shader_upscale &&
                (g_ddraw->render.viewport.width != g_ddraw->width || 
                    g_ddraw->render.viewport.height != g_ddraw->height) &&
                SUCCEEDED(IDirect3DDevice9_SetPixelShader(g_d3d9.device, g_d3d9.pixel_shader_upscale)))
            {
                float texture_size[4] = { (float)g_d3d9.tex_width, (float)g_d3d9.tex_height, 0, 0 };
                err = err || FAILED(IDirect3DDevice9_SetPixelShaderConstantF(g_d3d9.device, 0, texture_size, 1));
            }
        }
    }

    /*
    D3DVIEWPORT9 view_data = {
        g_ddraw->render.viewport.x,
        g_ddraw->render.viewport.y,
        g_ddraw->render.viewport.width,
        g_ddraw->render.viewport.height,
        0.0f,
        1.0f };

    err = err || FAILED(IDirect3DDevice9_SetViewport(g_d3d9.device, &view_data));
    */
    return !err;
}

static BOOL d3d9_update_vertices(BOOL upscale_hack, BOOL stretch)
{
    float vp_x = stretch ? (float)g_ddraw->render.viewport.x : 0.0f;
    float vp_y = stretch ? (float)g_ddraw->render.viewport.y : 0.0f;

    float vp_w = stretch ? (float)(g_ddraw->render.viewport.width + g_ddraw->render.viewport.x) : (float)g_ddraw->width;
    float vp_h = stretch ? (float)(g_ddraw->render.viewport.height + g_ddraw->render.viewport.y) : (float)g_ddraw->height;

    float s_h = upscale_hack ? g_d3d9.scale_h * ((float)g_ddraw->upscale_hack_height / g_ddraw->height) : g_d3d9.scale_h;
    float s_w = upscale_hack ? g_d3d9.scale_w * ((float)g_ddraw->upscale_hack_width / g_ddraw->width) : g_d3d9.scale_w;

    CUSTOMVERTEX vertices[] =
    {
        { vp_x - 0.5f, vp_h - 0.5f, 0.0f, 1.0f, 0.0f, s_h },
        { vp_x - 0.5f, vp_y - 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
        { vp_w - 0.5f, vp_h - 0.5f, 0.0f, 1.0f, s_w,  s_h },
        { vp_w - 0.5f, vp_y - 0.5f, 0.0f, 1.0f, s_w,  0.0f }
    };

    void* data;
    if (g_d3d9.vertex_buf && SUCCEEDED(IDirect3DVertexBuffer9_Lock(g_d3d9.vertex_buf, 0, 0, (void**)&data, 0)))
    {
        memcpy(data, vertices, sizeof(vertices));

        IDirect3DVertexBuffer9_Unlock(g_d3d9.vertex_buf);
        return TRUE;
    }

    return FALSE;
}

DWORD WINAPI d3d9_render_main(void)
{
    Sleep(250);

    fpsl_init();

    BOOL needs_update = FALSE;

    DWORD timeout = g_config.minfps > 0 ? g_ddraw->minfps_tick_len : INFINITE;

    while (g_ddraw->render.run &&
        (g_config.minfps < 0 || WaitForSingleObject(g_ddraw->render.sem, timeout) != WAIT_FAILED))
    {
#if _DEBUG
        dbg_draw_frame_info_start();
#endif

        static int tex_index = 0, pal_index = 0;

        fpsl_frame_start();

        EnterCriticalSection(&g_ddraw->cs);

        if (g_ddraw->primary && 
            g_ddraw->primary->bpp == g_ddraw->bpp &&
            g_ddraw->primary->width == g_ddraw->width &&
            g_ddraw->primary->height == g_ddraw->height &&
            (g_ddraw->bpp == 16 || g_ddraw->bpp == 32 || g_ddraw->primary->palette))
        {
            if (g_config.lock_surfaces)
                EnterCriticalSection(&g_ddraw->primary->cs);

            if (g_config.vhack)
            {
                if (util_detect_low_res_screen())
                {
                    if (!InterlockedExchange(&g_ddraw->upscale_hack_active, TRUE))
                        d3d9_update_vertices(TRUE, TRUE);
                }
                else
                {
                    if (InterlockedExchange(&g_ddraw->upscale_hack_active, FALSE))
                        d3d9_update_vertices(FALSE, TRUE);
                }
            }

            D3DLOCKED_RECT lock_rc;

            if (InterlockedExchange(&g_ddraw->render.surface_updated, FALSE) || g_config.minfps == -2)
            {
                if (++tex_index >= D3D9_TEXTURE_COUNT)
                    tex_index = 0;

                RECT rc = { 0, 0, g_ddraw->width, g_ddraw->height };

                if (SUCCEEDED(IDirect3DDevice9_SetTexture(g_d3d9.device, 0, (IDirect3DBaseTexture9*)g_d3d9.surface_tex[tex_index])) &&
                    SUCCEEDED(IDirect3DTexture9_LockRect(g_d3d9.surface_tex[tex_index], 0, &lock_rc, &rc, 0)))
                {
                    blt_clean(
                        lock_rc.pBits,
                        0,
                        0,
                        g_ddraw->primary->width,
                        g_ddraw->primary->height,
                        lock_rc.Pitch,
                        g_ddraw->primary->surface,
                        0,
                        0,
                        g_ddraw->primary->pitch,
                        g_ddraw->primary->bpp);

                    IDirect3DTexture9_UnlockRect(g_d3d9.surface_tex[tex_index], 0);
                }
            }

            if (g_ddraw->bpp == 8 &&
                (InterlockedExchange(&g_ddraw->render.palette_updated, FALSE) || g_config.minfps == -2))
            {
                if (++pal_index >= D3D9_TEXTURE_COUNT)
                    pal_index = 0;

                RECT rc = { 0,0,256,1 };

                if (SUCCEEDED(IDirect3DDevice9_SetTexture(g_d3d9.device, 1, (IDirect3DBaseTexture9*)g_d3d9.palette_tex[pal_index])) &&
                    SUCCEEDED(IDirect3DTexture9_LockRect(g_d3d9.palette_tex[pal_index], 0, &lock_rc, &rc, 0)))
                {
                    memcpy(lock_rc.pBits, g_ddraw->primary->palette->data_rgb, 256 * sizeof(int));

                    IDirect3DTexture9_UnlockRect(g_d3d9.palette_tex[pal_index], 0);
                }
            }

            if (g_config.fixchilds)
            {
                g_ddraw->child_window_exists = FALSE;
                InterlockedExchangePointer(&g_ddraw->video_window_hwnd, NULL);
                EnumChildWindows(g_ddraw->hwnd, util_enum_child_proc, (LPARAM)g_ddraw->primary);

                if (g_ddraw->render.width != g_ddraw->width || g_ddraw->render.height != g_ddraw->height)
                {
                    if (g_ddraw->child_window_exists)
                    {
                        IDirect3DDevice9_Clear(g_d3d9.device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

                        if (!needs_update && d3d9_update_vertices(FALSE, FALSE))
                            needs_update = TRUE;
                    }
                    else if (needs_update)
                    {
                        if (d3d9_update_vertices(FALSE, TRUE))
                            needs_update = FALSE;
                    }
                }
            }

            if (g_config.lock_surfaces)
                LeaveCriticalSection(&g_ddraw->primary->cs);
        }

        LeaveCriticalSection(&g_ddraw->cs);

        if (g_ddraw->render.viewport.x != 0 || g_ddraw->render.viewport.y != 0)
        {
            IDirect3DDevice9_Clear(g_d3d9.device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        }

        IDirect3DDevice9_BeginScene(g_d3d9.device);
        IDirect3DDevice9_DrawPrimitive(g_d3d9.device, D3DPT_TRIANGLESTRIP, 0, 2);
        IDirect3DDevice9_EndScene(g_d3d9.device);

        if (g_ddraw->bnet_active)
        {
            IDirect3DDevice9_Clear(g_d3d9.device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        }

        if (FAILED(IDirect3DDevice9_Present(g_d3d9.device, NULL, NULL, NULL, NULL)))
        {
            DWORD_PTR result;
            SendMessageTimeout(g_ddraw->hwnd, WM_D3D9DEVICELOST, 0, 0, 0, 1000, &result);

            ReleaseSemaphore(g_ddraw->render.sem, 1, NULL);
            Sleep(50);
        }

#if _DEBUG
        dbg_draw_frame_info_end();
#endif

        fpsl_frame_end();
    }

    if (g_config.vhack)
        InterlockedExchange(&g_ddraw->upscale_hack_active, FALSE);

    return 0;
}
