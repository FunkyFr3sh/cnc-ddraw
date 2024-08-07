#include <windows.h>
#include <initguid.h>
#include "directinput.h"
#include "winapi_hooks.h"
#include "debug.h"
#include "hook.h"
#include "dd.h"
#include "mouse.h"
#include "config.h"

#ifdef _MSC_VER
#include "detours.h"
#endif

BOOL g_dinput_hook_active;

DIRECTINPUTCREATEAPROC real_DirectInputCreateA;
DIRECTINPUTCREATEWPROC real_DirectInputCreateW;
DIRECTINPUTCREATEEXPROC real_DirectInputCreateEx;
DIRECTINPUT8CREATEPROC real_DirectInput8Create;

static DICREATEDEVICEPROC real_di_CreateDevice;
static DICREATEDEVICEEXPROC real_di_CreateDeviceEx;
static DIDSETCOOPERATIVELEVELPROC real_did_SetCooperativeLevel;
static DIDGETDEVICEDATAPROC real_did_GetDeviceData;
static DIDGETDEVICESTATEPROC real_did_GetDeviceState;

static PROC* g_di_CreateDevice_vtbl_addr;
static PROC* g_di_CreateDeviceEx_vtbl_addr;
static PROC* g_did_SetCooperativeLevel_vtbl_addr;
static PROC* g_did_GetDeviceData_vtbl_addr;
static PROC* g_did_GetDeviceState_vtbl_addr;

static LPDIRECTINPUTDEVICEA g_mouse_device;

static PROC hook_func(PROC* org_func, PROC new_func)
{
    if (!org_func || !new_func)
        return 0;

    PROC org = *org_func;
    DWORD old_protect;

    if (VirtualProtect(org_func, sizeof(PROC), PAGE_EXECUTE_READWRITE, &old_protect))
    {
        *org_func = new_func;
        VirtualProtect(org_func, sizeof(PROC), old_protect, &old_protect);

        return org;
    }

    return 0;
}

static HRESULT WINAPI fake_did_SetCooperativeLevel(IDirectInputDeviceA* This, HWND hwnd, DWORD dwFlags)
{
    TRACE("DirectInput SetCooperativeLevel(This=%p, hwnd=%p, dwFlags=0x%08X) [%p]\n", This, hwnd, dwFlags, _ReturnAddress());

    if (This == g_mouse_device && g_ddraw.ref && (dwFlags & DISCL_EXCLUSIVE))
    {
        if (g_mouse_locked || g_config.devmode)
        {
            while (real_ShowCursor(FALSE) >= 0);
        }

        InterlockedExchange((LONG*)&g_ddraw.show_cursor_count, -1);
    }

    return real_did_SetCooperativeLevel(This, hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
}

static HRESULT WINAPI fake_did_GetDeviceData(
    IDirectInputDeviceA* This,
    DWORD cbObjectData,
    LPDIDEVICEOBJECTDATA rgdod,
    LPDWORD pdwInOut,
    DWORD dwFlags)
{
    /*
    TRACE(
        "DirectInput GetDeviceData(This=%p, cbObjectData=%lu, rgdod=%p, pdwInOut=%p, dwFlags=%lu) [%p]\n", 
        This,
        cbObjectData,
        rgdod,
        pdwInOut,
        dwFlags,
        _ReturnAddress());
        */

    HRESULT result = real_did_GetDeviceData(This, cbObjectData, rgdod, pdwInOut, dwFlags);

    if (SUCCEEDED(result) && This == g_mouse_device)
    {
        if (pdwInOut && rgdod && *pdwInOut > 0 && cbObjectData > 0)
        {
            if (!g_mouse_locked && !g_config.devmode)
            {
                memset(rgdod, 0, *pdwInOut * cbObjectData);
                *pdwInOut = 0;
            }
            else if (g_ddraw.width && g_ddraw.height)
            {
                POINT pt;
                fake_GetCursorPos(&pt);
                fake_SetCursorPos((g_ddraw.width / 2), (g_ddraw.height / 2));

                static int x, y;
                x += pt.x - (g_ddraw.width / 2);
                y += pt.y - (g_ddraw.height / 2);

                if (cbObjectData == sizeof(DIDEVICEOBJECTDATA_DX3))
                {
                    LPDIDEVICEOBJECTDATA_DX3 rgdod16 = (LPDIDEVICEOBJECTDATA_DX3)rgdod;

                    for (int i = 0; i < *pdwInOut; i++)
                    {
                        if (rgdod16[i].dwOfs == DIMOFS_X)
                        {
                            rgdod16[i].dwData = x;
                            x = 0;
                        }
                        else if (rgdod16[i].dwOfs == DIMOFS_Y)
                        {
                            rgdod16[i].dwData = y;
                            y = 0;
                        }
                    }
                }
                else if (cbObjectData == sizeof(DIDEVICEOBJECTDATA))
                {
                    for (int i = 0; i < *pdwInOut; i++)
                    {
                        if (rgdod[i].dwOfs == DIMOFS_X)
                        {
                            rgdod[i].dwData = x;
                            x = 0;
                        }
                        else if (rgdod[i].dwOfs == DIMOFS_Y)
                        {
                            rgdod[i].dwData = y;
                            y = 0;
                        }
                    }
                }
            }
        }
    }

    return result;
}

static HRESULT WINAPI fake_did_GetDeviceState(IDirectInputDeviceA* This, DWORD cbData, LPVOID lpvData)
{
    //TRACE("DirectInput GetDeviceState(This=%p, cbData=%lu, lpvData=%p) [%p]\n", This, cbData, lpvData, _ReturnAddress());

    HRESULT result = real_did_GetDeviceState(This, cbData, lpvData);

    if (SUCCEEDED(result) && This == g_mouse_device && !g_mouse_locked && !g_config.devmode)
    {
        if (cbData > 0 && lpvData)
        {
            memset(lpvData, 0, cbData);
        }
    }

    return result;
}

static HRESULT WINAPI fake_di_CreateDevice(
    IDirectInputA* This,
    REFGUID rguid,
    LPDIRECTINPUTDEVICEA* lplpDIDevice,
    LPUNKNOWN pUnkOuter)
{
    TRACE("DirectInput CreateDevice [%p]\n", _ReturnAddress());

    HRESULT result = real_di_CreateDevice(This, rguid, lplpDIDevice, pUnkOuter);

    if (SUCCEEDED(result))
    {
        if (rguid && IsEqualGUID(&GUID_SysMouse, rguid))
        {
            g_mouse_device = *lplpDIDevice;
        }

        if (!real_did_SetCooperativeLevel)
        {
            g_did_SetCooperativeLevel_vtbl_addr = (PROC*)&(*lplpDIDevice)->lpVtbl->SetCooperativeLevel;

            real_did_SetCooperativeLevel =
                (DIDSETCOOPERATIVELEVELPROC)hook_func(g_did_SetCooperativeLevel_vtbl_addr, (PROC)fake_did_SetCooperativeLevel);

            g_did_GetDeviceData_vtbl_addr = (PROC*)&(*lplpDIDevice)->lpVtbl->GetDeviceData;

            real_did_GetDeviceData =
                (DIDGETDEVICEDATAPROC)hook_func(g_did_GetDeviceData_vtbl_addr, (PROC)fake_did_GetDeviceData);

            g_did_GetDeviceState_vtbl_addr = (PROC*)&(*lplpDIDevice)->lpVtbl->GetDeviceState;

            real_did_GetDeviceState =
                (DIDGETDEVICESTATEPROC)hook_func(g_did_GetDeviceState_vtbl_addr, (PROC)fake_did_GetDeviceState);
        }
    }

    return result;
}

static HRESULT WINAPI fake_di_CreateDeviceEx(
    IDirectInputA* This,
    REFGUID rguid,
    REFIID riid,
    LPDIRECTINPUTDEVICEA* lplpDIDevice,
    LPUNKNOWN pUnkOuter)
{
    TRACE("DirectInput CreateDeviceEx [%p]\n", _ReturnAddress());

    HRESULT result = real_di_CreateDeviceEx(This, rguid, riid, lplpDIDevice, pUnkOuter);

    if (SUCCEEDED(result))
    {
        if (rguid && IsEqualGUID(&GUID_SysMouse, rguid))
        {
            g_mouse_device = *lplpDIDevice;
        }

        if (!real_did_SetCooperativeLevel)
        {
            g_did_SetCooperativeLevel_vtbl_addr = (PROC*)&(*lplpDIDevice)->lpVtbl->SetCooperativeLevel;

            real_did_SetCooperativeLevel =
                (DIDSETCOOPERATIVELEVELPROC)hook_func(g_did_SetCooperativeLevel_vtbl_addr, (PROC)fake_did_SetCooperativeLevel);

            g_did_GetDeviceData_vtbl_addr = (PROC*)&(*lplpDIDevice)->lpVtbl->GetDeviceData;

            real_did_GetDeviceData =
                (DIDGETDEVICEDATAPROC)hook_func(g_did_GetDeviceData_vtbl_addr, (PROC)fake_did_GetDeviceData);

            g_did_GetDeviceState_vtbl_addr = (PROC*)&(*lplpDIDevice)->lpVtbl->GetDeviceState;

            real_did_GetDeviceState =
                (DIDGETDEVICESTATEPROC)hook_func(g_did_GetDeviceState_vtbl_addr, (PROC)fake_did_GetDeviceState);
        }
    }

    return result;
}

HRESULT WINAPI fake_DirectInputCreateA(
    HINSTANCE hinst,
    DWORD dwVersion,
    LPDIRECTINPUTA* lplpDirectInput,
    LPUNKNOWN punkOuter)
{
    TRACE("DirectInputCreateA [%p]\n", _ReturnAddress());

    if (!real_DirectInputCreateA)
    {
        real_DirectInputCreateA =
            (DIRECTINPUTCREATEAPROC)real_GetProcAddress(real_LoadLibraryA("dinput.dll"), "DirectInputCreateA");

        if (real_DirectInputCreateA == fake_DirectInputCreateA)
        {
            real_DirectInputCreateA =
                (DIRECTINPUTCREATEAPROC)real_GetProcAddress(
                    real_LoadLibraryA("system32\\dinput.dll"), 
                    "DirectInputCreateA");
        }
    }

    if (!real_DirectInputCreateA)
        return DIERR_GENERIC;

    HRESULT result = real_DirectInputCreateA(hinst, dwVersion, lplpDirectInput, punkOuter);

    if (SUCCEEDED(result) && !real_di_CreateDevice && !g_config.no_dinput_hook)
    {
        g_di_CreateDevice_vtbl_addr = (PROC*)&(*lplpDirectInput)->lpVtbl->CreateDevice;

        real_di_CreateDevice =
            (DICREATEDEVICEPROC)hook_func(g_di_CreateDevice_vtbl_addr, (PROC)fake_di_CreateDevice);
    }

    return result;
}

HRESULT WINAPI fake_DirectInputCreateW(
    HINSTANCE hinst,
    DWORD dwVersion,
    LPDIRECTINPUTW* lplpDirectInput,
    LPUNKNOWN punkOuter)
{
    TRACE("DirectInputCreateW [%p]\n", _ReturnAddress());

    if (!real_DirectInputCreateW)
    {
        real_DirectInputCreateW =
            (DIRECTINPUTCREATEWPROC)real_GetProcAddress(real_LoadLibraryA("dinput.dll"), "DirectInputCreateW");

        if (real_DirectInputCreateW == fake_DirectInputCreateW)
        {
            real_DirectInputCreateW =
                (DIRECTINPUTCREATEWPROC)real_GetProcAddress(
                    real_LoadLibraryA("system32\\dinput.dll"),
                    "DirectInputCreateW");
        }
    }

    if (!real_DirectInputCreateW)
        return DIERR_GENERIC;

    HRESULT result = real_DirectInputCreateW(hinst, dwVersion, lplpDirectInput, punkOuter);

    if (SUCCEEDED(result) && !real_di_CreateDevice && !g_config.no_dinput_hook)
    {
        g_di_CreateDevice_vtbl_addr = (PROC*)&(*lplpDirectInput)->lpVtbl->CreateDevice;

        real_di_CreateDevice =
            (DICREATEDEVICEPROC)hook_func(g_di_CreateDevice_vtbl_addr, (PROC)fake_di_CreateDevice);
    }

    return result;
}

HRESULT WINAPI fake_DirectInputCreateEx(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPDIRECTINPUT7A* ppvOut,
    LPUNKNOWN punkOuter)
{
    TRACE("DirectInputCreateEx [%p]\n", _ReturnAddress());

    if (!real_DirectInputCreateEx)
    {
        real_DirectInputCreateEx =
            (DIRECTINPUTCREATEEXPROC)real_GetProcAddress(real_LoadLibraryA("dinput.dll"), "DirectInputCreateEx");

        if (real_DirectInputCreateEx == fake_DirectInputCreateEx)
        {
            real_DirectInputCreateEx =
                (DIRECTINPUTCREATEEXPROC)real_GetProcAddress(
                    real_LoadLibraryA("system32\\dinput.dll"),
                    "DirectInputCreateEx");
        }
    }

    if (!real_DirectInputCreateEx)
        return DIERR_GENERIC;

    HRESULT result = real_DirectInputCreateEx(hinst, dwVersion, riidltf, ppvOut, punkOuter);

    if (SUCCEEDED(result) && !real_di_CreateDevice && !g_config.no_dinput_hook)
    {
        g_di_CreateDevice_vtbl_addr = (PROC*)&(*ppvOut)->lpVtbl->CreateDevice;

        real_di_CreateDevice =
            (DICREATEDEVICEPROC)hook_func(g_di_CreateDevice_vtbl_addr, (PROC)fake_di_CreateDevice);
    }

    if (SUCCEEDED(result) &&
        !real_di_CreateDeviceEx &&
        riidltf &&
        (IsEqualGUID(&IID_IDirectInput7A, riidltf) || IsEqualGUID(&IID_IDirectInput7W, riidltf)) 
        && !g_config.no_dinput_hook)
    {
        g_di_CreateDeviceEx_vtbl_addr = (PROC*)&(*ppvOut)->lpVtbl->CreateDeviceEx;

        real_di_CreateDeviceEx =
            (DICREATEDEVICEEXPROC)hook_func(g_di_CreateDeviceEx_vtbl_addr, (PROC)fake_di_CreateDeviceEx);
    }

    return result;
}

HRESULT WINAPI fake_DirectInput8Create(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPDIRECTINPUT8* ppvOut,
    LPUNKNOWN punkOuter)
{
    TRACE("DirectInput8Create [%p]\n", _ReturnAddress());

    if (!real_DirectInput8Create)
    {
        real_DirectInput8Create =
            (DIRECTINPUT8CREATEPROC)real_GetProcAddress(real_LoadLibraryA("dinput8.dll"), "DirectInput8Create");

        if (real_DirectInput8Create == fake_DirectInput8Create)
        {
            real_DirectInput8Create =
                (DIRECTINPUT8CREATEPROC)real_GetProcAddress(
                    real_LoadLibraryA("system32\\dinput8.dll"), 
                    "DirectInput8Create");
        }
    }

    if (!real_DirectInput8Create)
        return DIERR_GENERIC;

    HRESULT result = real_DirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);

    if (SUCCEEDED(result) && !real_di_CreateDevice && !g_config.no_dinput_hook)
    {
        g_di_CreateDevice_vtbl_addr = (PROC*)&(*ppvOut)->lpVtbl->CreateDevice;

        real_di_CreateDevice =
            (DICREATEDEVICEPROC)hook_func(g_di_CreateDevice_vtbl_addr, (PROC)fake_di_CreateDevice);
    }

    return result;
}

void dinput_hook_init()
{
#ifdef _MSC_VER
    if (!g_dinput_hook_active)
    {
        g_dinput_hook_active = TRUE;

        real_DirectInputCreateA = (void*)real_GetProcAddress(real_LoadLibraryA("dinput.dll"), "DirectInputCreateA");

        if (real_DirectInputCreateA && real_DirectInputCreateA != fake_DirectInputCreateA)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach((PVOID*)&real_DirectInputCreateA, (PVOID)fake_DirectInputCreateA);
            DetourTransactionCommit();
        }
        /* Being called from winmm for some reason
        real_DirectInputCreateW = (void*)real_GetProcAddress(real_LoadLibraryA("dinput.dll"), "DirectInputCreateW");

        if (real_DirectInputCreateW && real_DirectInputCreateW != fake_DirectInputCreateW)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach((PVOID*)&real_DirectInputCreateW, (PVOID)fake_DirectInputCreateW);
            DetourTransactionCommit();
        }
        */
        real_DirectInputCreateEx = (void*)real_GetProcAddress(real_LoadLibraryA("dinput.dll"), "DirectInputCreateEx");

        if (real_DirectInputCreateEx && real_DirectInputCreateEx != fake_DirectInputCreateEx)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach((PVOID*)&real_DirectInputCreateEx, (PVOID)fake_DirectInputCreateEx);
            DetourTransactionCommit();
        }

        real_DirectInput8Create = (void*)real_GetProcAddress(real_LoadLibraryA("dinput8.dll"), "DirectInput8Create");

        if (real_DirectInput8Create && real_DirectInput8Create != fake_DirectInput8Create)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach((PVOID*)&real_DirectInput8Create, (PVOID)fake_DirectInput8Create);
            DetourTransactionCommit();
        }
    }
#endif
}

void dinput_hook_exit()
{
    hook_func(g_di_CreateDevice_vtbl_addr, (PROC)real_di_CreateDevice);
    hook_func(g_di_CreateDeviceEx_vtbl_addr, (PROC)real_di_CreateDeviceEx);
    hook_func(g_did_SetCooperativeLevel_vtbl_addr, (PROC)real_did_SetCooperativeLevel);
    hook_func(g_did_GetDeviceData_vtbl_addr, (PROC)real_did_GetDeviceData);
    hook_func(g_did_GetDeviceState_vtbl_addr, (PROC)real_did_GetDeviceState);

#ifdef _MSC_VER
    if (g_dinput_hook_active)
    {
        if (real_DirectInputCreateA)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach((PVOID*)&real_DirectInputCreateA, (PVOID)fake_DirectInputCreateA);
            DetourTransactionCommit();
        }
        /* Being called from winmm for some reason
        if (real_DirectInputCreateW)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach((PVOID*)&real_DirectInputCreateW, (PVOID)fake_DirectInputCreateW);
            DetourTransactionCommit();
        }
        */
        if (real_DirectInputCreateEx)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach((PVOID*)&real_DirectInputCreateEx, (PVOID)fake_DirectInputCreateEx);
            DetourTransactionCommit();
        }

        if (real_DirectInput8Create)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach((PVOID*)&real_DirectInput8Create, (PVOID)fake_DirectInput8Create);
            DetourTransactionCommit();
        }

        g_dinput_hook_active = FALSE;
    }
#endif
}
