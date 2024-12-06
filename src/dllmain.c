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
#include "patch.h"


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
        cfg_load();

        if (GetEnvironmentVariableW(L"cnc_ddraw_config_init", NULL, 0))
            return TRUE;

#ifdef _DEBUG 
        dbg_init();
        TRACE("cnc-ddraw = %p\n", hDll);
        g_dbg_exception_filter = real_SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)dbg_exception_handler);
#endif
        g_ddraw_module = hDll;



        /* Claw DVD Movie experiments */

        /* change file extension .vob to .avi */
        HMODULE game_exe = GetModuleHandleA(NULL);

        PIMAGE_DOS_HEADER dos_hdr = (void*)game_exe;
        PIMAGE_NT_HEADERS nt_hdr = (void*)((char*)game_exe + dos_hdr->e_lfanew);

        for (int i = 0; i < nt_hdr->FileHeader.NumberOfSections; i++)
        {
            PIMAGE_SECTION_HEADER sct_hdr = IMAGE_FIRST_SECTION(nt_hdr) + i;

            if (strcmp(".data", (char*)sct_hdr->Name) == 0)
            {
                char* s = (char*)((char*)game_exe + sct_hdr->VirtualAddress);
                int s_len = sct_hdr->Misc.VirtualSize;

                for (int i = 0; i < s_len; i++, s++)
                {
                    if (*s == '.' && memcmp(s, "\x2E\x76\x6F\x62\x00", 5) == 0) /* .vob */
                    {
                        memcpy(s, "\x2E\x61\x76\x69", 4);  /* .avi */
                    }
                }

                break;
            }
            sct_hdr++;
        }

        /* add registry key for x264vfw */

        HKEY hkey;
        LONG status =
            RegCreateKeyExA(
                HKEY_CURRENT_USER,
                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32",
                0,
                NULL,
                REG_OPTION_NON_VOLATILE,
                KEY_WRITE | KEY_QUERY_VALUE,
                NULL,
                &hkey,
                NULL);

        if (status == ERROR_SUCCESS)
        {
            LPCTSTR x264 = "x264vfw.dll";
            RegSetValueExA(hkey,"vidc.x264", 0, REG_SZ, (const BYTE*)x264, strlen(x264) + 1);

            LPCTSTR xvid = "xvidvfw.dll";
            RegSetValueExA(hkey, "vidc.xvid", 0, REG_SZ, (const BYTE*)xvid, strlen(xvid) + 1);

            RegCloseKey(hkey);
        }





        char buf[1024];

        if (GetEnvironmentVariable("__COMPAT_LAYER", buf, sizeof(buf)))
        {
            TRACE("__COMPAT_LAYER = %s\n", buf);

            char* s = strtok(buf, " ");

            while (s)
            {
                if (_strcmpi(s, "WIN95") == 0 || _strcmpi(s, "WIN98") == 0 || _strcmpi(s, "NT4SP5") == 0)
                {
                    char mes[128] = { 0 };

                    _snprintf(
                        mes,
                        sizeof(mes),
                        "Please disable the '%s' compatibility mode for all game executables and "
                        "then try to start the game again.",
                        s);

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

        timeBeginPeriod(1);
        hook_init(TRUE);
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        if (GetEnvironmentVariableW(L"cnc_ddraw_config_init", NULL, 0))
            return TRUE;

        TRACE("cnc-ddraw DLL_PROCESS_DETACH\n");

        cfg_save();

        timeEndPeriod(1);
        dinput_hook_exit();
        hook_exit();
        break;
    }
    }

    return TRUE;
}

HRESULT WINAPI DirectDrawCreate(GUID FAR* lpGUID, LPDIRECTDRAW FAR* lplpDD, IUnknown FAR* pUnkOuter)
{
    TRACE("-> %s(lpGUID=%p, lplpDD=%p, pUnkOuter=%p)\n", __FUNCTION__, lpGUID, lplpDD, pUnkOuter);
    HRESULT ret = dd_CreateEx(lpGUID, (LPVOID*)lplpDD, &IID_IDirectDraw, pUnkOuter);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR* lplpDDClipper, IUnknown FAR* pUnkOuter)
{
    TRACE("-> %s(dwFlags=%08X, DDClipper=%p, unkOuter=%p)\n", __FUNCTION__, (int)dwFlags, lplpDDClipper, pUnkOuter);
    HRESULT ret = dd_CreateClipper(dwFlags, (IDirectDrawClipperImpl**)lplpDDClipper, pUnkOuter);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI DirectDrawCreateEx(GUID* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown* pUnkOuter)
{
    TRACE("-> %s(lpGUID=%p, lplpDD=%p, riid=%08X, pUnkOuter=%p)\n", __FUNCTION__, lpGuid, lplpDD, iid, pUnkOuter);
    HRESULT ret = dd_CreateEx(lpGuid, lplpDD, &IID_IDirectDraw7, pUnkOuter);
    TRACE("<- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACK lpCallback, LPVOID lpContext)
{
    TRACE("-> %s(lpCallback=%p, lpContext=%p)\n", __FUNCTION__, lpCallback, lpContext);

    if (lpCallback)
        lpCallback(NULL, "Primary Display Driver", "display", lpContext);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags)
{
    TRACE("-> %s(lpCallback=%p, lpContext=%p, dwFlags=%d)\n", __FUNCTION__, lpCallback, lpContext, dwFlags);

    if (lpCallback)
        lpCallback(NULL, "Primary Display Driver", "display", lpContext, NULL);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags)
{
    TRACE("-> %s(lpCallback=%p, lpContext=%p, dwFlags=%d)\n", __FUNCTION__, lpCallback, lpContext, dwFlags);

    if (lpCallback)
        lpCallback(NULL, L"Primary Display Driver", L"display", lpContext, NULL);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateW(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext)
{
    TRACE("-> %s(lpCallback=%p, lpContext=%p)\n", __FUNCTION__, lpCallback, lpContext);

    if (lpCallback)
        lpCallback(NULL, L"Primary Display Driver", L"display", lpContext);

    TRACE("<- %s\n", __FUNCTION__);
    return DD_OK;
}

DWORD WINAPI CompleteCreateSysmemSurface(DWORD a)
{
    TRACE("NOT_IMPLEMENTED -> %s()\n", __FUNCTION__);
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

HRESULT WINAPI D3DParseUnknownCommand(LPVOID lpCmd, LPVOID* lpRetCmd)
{
    TRACE("NOT_IMPLEMENTED -> %s(lpCmd=%p, lpRetCmd=%p)\n", __FUNCTION__, lpCmd, lpRetCmd);
    HRESULT ret = E_FAIL;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI AcquireDDThreadLock()
{
    TRACE("NOT_IMPLEMENTED -> %s()\n", __FUNCTION__);
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI ReleaseDDThreadLock()
{
    TRACE("NOT_IMPLEMENTED -> %s()\n", __FUNCTION__);
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI DDInternalLock(DWORD a, DWORD b)
{
    TRACE("NOT_IMPLEMENTED -> %s()\n", __FUNCTION__);
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}

DWORD WINAPI DDInternalUnlock(DWORD a)
{
    TRACE("NOT_IMPLEMENTED -> %s()\n", __FUNCTION__);
    DWORD ret = 0;
    TRACE("NOT_IMPLEMENTED <- %s\n", __FUNCTION__);
    return ret;
}
