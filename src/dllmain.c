#include <windows.h>
#include "ddraw.h"
#include <stdio.h>
#include "dllmain.h"
#include "directinput.h"
#include "IDirectDraw.h"
#include "dd.h"
#include "ddclipper.h"
#include "debug.h"
#include "config.h"
#include "hook.h"
#include "indeo.h"
#include "utils.h"
#include "versionhelpers.h"


/* export for cncnet cnc games */
BOOL GameHandlesClose;

/* export for some warcraft II tools */
PVOID FakePrimarySurface;


HMODULE g_ddraw_module;

BOOL WINAPI DllMain(HANDLE hDll, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_ddraw_module = hDll;

        verhelp_init();

        if (GetEnvironmentVariableW(L"cnc_ddraw_config_init", NULL, 0))
        {
            cfg_load();
            return TRUE;
        }

#ifdef _DEBUG 
        dbg_init();
        TRACE("cnc-ddraw = %p\n", hDll);
        g_dbg_exception_filter = real_SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)dbg_exception_handler);
#endif

        cfg_load();

        char buf[1024];

        if (GetEnvironmentVariable("__COMPAT_LAYER", buf, sizeof(buf)))
        {
            TRACE("__COMPAT_LAYER = %s\n", buf);

            char* s = strtok(buf, " ");

            while (s)
            {
                /* Workaround for bug in Windows 11 (Steam RA2 crash) */
                if (_strcmpi(s, "Win7RTM") == 0)
                {
                    g_config.d3d9on12 = TRUE;
                }

                if (_strcmpi(s, "WIN95") == 0 || _strcmpi(s, "WIN98") == 0 || _strcmpi(s, "NT4SP5") == 0)
                {
                    char mes[128] = { 0 };

                    _snprintf(
                        mes,
                        sizeof(mes) - 1,
                        "Please disable the '%s' compatibility mode for all game executables and "
                        "then try to start the game again.",
                        s);

                    if (!g_config.no_compat_warning)
                        MessageBoxA(NULL, mes, "Compatibility modes detected - cnc-ddraw", MB_OK);

                    break;
                }

                s = strtok(NULL, " ");
            }
        }

        BOOL set_dpi_aware = FALSE;

        HMODULE shcore_dll = GetModuleHandle("shcore.dll");
        HMODULE user32_dll = GetModuleHandle("user32.dll");

        if (user32_dll)
        {
            SETPROCESSDPIAWARENESSCONTEXTPROC set_awareness_context =
                (SETPROCESSDPIAWARENESSCONTEXTPROC)real_GetProcAddress(user32_dll, "SetProcessDpiAwarenessContext");

            if (set_awareness_context)
            {
                set_dpi_aware = set_awareness_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            }
        }

        if (!set_dpi_aware && shcore_dll)
        {
            SETPROCESSDPIAWERENESSPROC set_awareness =
                (SETPROCESSDPIAWERENESSPROC)real_GetProcAddress(shcore_dll, "SetProcessDpiAwareness");

            if (set_awareness)
            {
                HRESULT result = set_awareness(PROCESS_PER_MONITOR_DPI_AWARE);

                set_dpi_aware = result == S_OK || result == E_ACCESSDENIED;
            }
        }

        if (!set_dpi_aware && user32_dll)
        {
            SETPROCESSDPIAWAREPROC set_aware =
                (SETPROCESSDPIAWAREPROC)real_GetProcAddress(user32_dll, "SetProcessDPIAware");

            if (set_aware)
                set_aware();
        }

        indeo_enable();
        timeBeginPeriod(1);
        hook_init();
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        if (GetEnvironmentVariableW(L"cnc_ddraw_config_init", NULL, 0))
            return TRUE;

        TRACE("cnc-ddraw DLL_PROCESS_DETACH\n");

        cfg_save();

        indeo_disable();
        timeEndPeriod(1);
        dinput_hook_exit();
        hook_exit();
        break;
    }
    }

    return TRUE;
}

void DDEnableZoom()
{
    TRACE("%s [%p]\n", __FUNCTION__, _ReturnAddress());

    g_ddraw.zoom.enabled = TRUE;
    InterlockedExchange(&g_ddraw.zoom.frame_skip, 20);
}

BOOL DDIsWindowed()
{
    TRACE("%s [%p]\n", __FUNCTION__, _ReturnAddress());

    return g_config.windowed && !g_config.fullscreen;
}

FARPROC WINAPI DDGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    TRACE("%s [%p]\n", __FUNCTION__, _ReturnAddress());

    return real_GetProcAddress(hModule, lpProcName);
}

HRESULT WINAPI DirectDrawCreate(GUID FAR* lpGUID, LPDIRECTDRAW FAR* lplpDD, IUnknown FAR* pUnkOuter)
{
    TRACE("-> %s(lpGUID=%p, lplpDD=%p, pUnkOuter=%p) [%p]\n", __FUNCTION__, lpGUID, lplpDD, pUnkOuter, _ReturnAddress());

    HRESULT ret;

    if (util_caller_is_ddraw_wrapper(_ReturnAddress()))
    {
        if (lplpDD)
            *lplpDD = NULL;

        ret = DDERR_GENERIC;
    }
    else
    {
        ret = dd_CreateEx(lpGUID, (LPVOID*)lplpDD, &IID_IDirectDraw, pUnkOuter);
    }

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR* lplpDDClipper, IUnknown FAR* pUnkOuter)
{
    TRACE(
        "-> %s(dwFlags=%08X, DDClipper=%p, unkOuter=%p) [%p]\n", 
        __FUNCTION__,
        (int)dwFlags,
        lplpDDClipper, 
        pUnkOuter, 
        _ReturnAddress());

    HRESULT ret = dd_CreateClipper(dwFlags, (IDirectDrawClipperImpl**)lplpDDClipper, pUnkOuter);

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI DirectDrawCreateEx(GUID* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown* pUnkOuter)
{
    TRACE(
        "-> %s(lpGUID=%p, lplpDD=%p, riid=%08X, pUnkOuter=%p) [%p]\n",
        __FUNCTION__, 
        lpGuid,
        lplpDD, 
        iid,
        pUnkOuter, 
        _ReturnAddress());

    HRESULT ret;

    if (util_caller_is_ddraw_wrapper(_ReturnAddress()))
    {
        if (lplpDD)
            *lplpDD = NULL;

        ret = DDERR_GENERIC;
    }
    else
    {
        ret = dd_CreateEx(lpGuid, lplpDD, &IID_IDirectDraw7, pUnkOuter);
    }

    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACK lpCallback, LPVOID lpContext)
{
    TRACE("-> %s(lpCallback=%p, lpContext=%p) [%p]\n", __FUNCTION__, lpCallback, lpContext, _ReturnAddress());

    if (lpCallback)
        lpCallback(NULL, "Primary Display Driver", "display", lpContext);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags)
{
    TRACE(
        "-> %s(lpCallback=%p, lpContext=%p, dwFlags=%d) [%p]\n", 
        __FUNCTION__, 
        lpCallback, 
        lpContext, 
        dwFlags, 
        _ReturnAddress());

    if (lpCallback)
        lpCallback(NULL, "Primary Display Driver", "display", lpContext, NULL);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags)
{
    TRACE(
        "-> %s(lpCallback=%p, lpContext=%p, dwFlags=%d) [%p]\n", __FUNCTION__, 
        lpCallback, 
        lpContext, 
        dwFlags, 
        _ReturnAddress());

    if (lpCallback)
        lpCallback(NULL, L"Primary Display Driver", L"display", lpContext, NULL);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateW(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext)
{
    TRACE("-> %s(lpCallback=%p, lpContext=%p) [%p]\n", __FUNCTION__, lpCallback, lpContext, _ReturnAddress());

    if (lpCallback)
        lpCallback(NULL, L"Primary Display Driver", L"display", lpContext);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

DWORD WINAPI CompleteCreateSysmemSurface(DWORD a)
{
    TRACE("NOT_IMPLEMENTED -> %s() [%p]\n", __FUNCTION__, _ReturnAddress());
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI D3DParseUnknownCommand(LPVOID lpCmd, LPVOID* lpRetCmd)
{
    TRACE("NOT_IMPLEMENTED -> %s(lpCmd=%p, lpRetCmd=%p) [%p]\n", __FUNCTION__, lpCmd, lpRetCmd, _ReturnAddress());
    HRESULT ret = E_FAIL;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI AcquireDDThreadLock()
{
    TRACE("NOT_IMPLEMENTED -> %s() [%p]\n", __FUNCTION__, _ReturnAddress());
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI ReleaseDDThreadLock()
{
    TRACE("NOT_IMPLEMENTED -> %s() [%p]\n", __FUNCTION__, _ReturnAddress());
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI DDInternalLock(DWORD a, DWORD b)
{
    TRACE("NOT_IMPLEMENTED -> %s() [%p]\n", __FUNCTION__, _ReturnAddress());
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI DDInternalUnlock(DWORD a)
{
    TRACE("NOT_IMPLEMENTED -> %s() [%p]\n", __FUNCTION__, _ReturnAddress());
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}
