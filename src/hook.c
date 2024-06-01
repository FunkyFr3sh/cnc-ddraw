#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include "directinput.h"
#include "dd.h"
#include "winapi_hooks.h"
#include "hook.h"
#include "debug.h"
#include "dllmain.h"
#include "config.h"

#ifdef _MSC_VER
#include "detours.h"
#endif

BOOL g_hook_active;

GETCURSORPOSPROC real_GetCursorPos = GetCursorPos;
CLIPCURSORPROC real_ClipCursor = ClipCursor;
SHOWCURSORPROC real_ShowCursor = ShowCursor;
SETCURSORPROC real_SetCursor = SetCursor;
GETWINDOWRECTPROC real_GetWindowRect = GetWindowRect;
GETCLIENTRECTPROC real_GetClientRect = GetClientRect;
CLIENTTOSCREENPROC real_ClientToScreen = ClientToScreen;
SCREENTOCLIENTPROC real_ScreenToClient = ScreenToClient;
SETCURSORPOSPROC real_SetCursorPos = SetCursorPos;
WINDOWFROMPOINTPROC real_WindowFromPoint = WindowFromPoint;
GETCLIPCURSORPROC real_GetClipCursor = GetClipCursor;
GETCURSORINFOPROC real_GetCursorInfo = GetCursorInfo;
GETSYSTEMMETRICSPROC real_GetSystemMetrics = GetSystemMetrics;
SETWINDOWPOSPROC real_SetWindowPos = SetWindowPos;
MOVEWINDOWPROC real_MoveWindow = MoveWindow;
SENDMESSAGEAPROC real_SendMessageA = SendMessageA;
SETWINDOWLONGAPROC real_SetWindowLongA = SetWindowLongA;
GETWINDOWLONGAPROC real_GetWindowLongA = GetWindowLongA;
ENABLEWINDOWPROC real_EnableWindow = EnableWindow;
CREATEWINDOWEXAPROC real_CreateWindowExA = CreateWindowExA;
DESTROYWINDOWPROC real_DestroyWindow = DestroyWindow;
MAPWINDOWPOINTSPROC real_MapWindowPoints = MapWindowPoints;
SHOWWINDOWPROC real_ShowWindow = ShowWindow;
GETTOPWINDOWPROC real_GetTopWindow = GetTopWindow;
GETFOREGROUNDWINDOWPROC real_GetForegroundWindow = GetForegroundWindow;
STRETCHBLTPROC real_StretchBlt = StretchBlt;
SETDIBITSTODEVICEPROC real_SetDIBitsToDevice = SetDIBitsToDevice;
STRETCHDIBITSPROC real_StretchDIBits = StretchDIBits;
SETFOREGROUNDWINDOWPROC real_SetForegroundWindow = SetForegroundWindow;
SETWINDOWSHOOKEXAPROC real_SetWindowsHookExA = SetWindowsHookExA;
PEEKMESSAGEAPROC real_PeekMessageA = PeekMessageA;
GETDEVICECAPSPROC real_GetDeviceCaps = GetDeviceCaps;
CREATEFONTINDIRECTAPROC real_CreateFontIndirectA = CreateFontIndirectA;
CREATEFONTAPROC real_CreateFontA = CreateFontA;
LOADLIBRARYAPROC real_LoadLibraryA = LoadLibraryA;
LOADLIBRARYWPROC real_LoadLibraryW = LoadLibraryW;
LOADLIBRARYEXAPROC real_LoadLibraryExA = LoadLibraryExA;
LOADLIBRARYEXWPROC real_LoadLibraryExW = LoadLibraryExW;
GETPROCADDRESSPROC real_GetProcAddress = GetProcAddress;
GETDISKFREESPACEAPROC real_GetDiskFreeSpaceA = GetDiskFreeSpaceA;
COCREATEINSTANCEPROC real_CoCreateInstance = CoCreateInstance;
MCISENDCOMMANDAPROC real_mciSendCommandA = mciSendCommandA;
SETUNHANDLEDEXCEPTIONFILTERPROC real_SetUnhandledExceptionFilter = SetUnhandledExceptionFilter;

HOOKLIST g_hook_hooklist[] =
{
    {
        "user32.dll",
        {
            { "GetCursorPos", (PROC)fake_GetCursorPos, (PROC*)&real_GetCursorPos, 0 },
            { "ClipCursor", (PROC)fake_ClipCursor, (PROC*)&real_ClipCursor, 0 },
            { "ShowCursor", (PROC)fake_ShowCursor, (PROC*)&real_ShowCursor, 0 },
            { "SetCursor", (PROC)fake_SetCursor, (PROC*)&real_SetCursor, 0 },
            { "GetWindowRect", (PROC)fake_GetWindowRect, (PROC*)&real_GetWindowRect, 0 },
            { "GetClientRect", (PROC)fake_GetClientRect, (PROC*)&real_GetClientRect, 0 },
            { "ClientToScreen", (PROC)fake_ClientToScreen, (PROC*)&real_ClientToScreen, 0 },
            { "ScreenToClient", (PROC)fake_ScreenToClient, (PROC*)&real_ScreenToClient, 0 },
            { "SetCursorPos", (PROC)fake_SetCursorPos, (PROC*)&real_SetCursorPos, 0 },
            { "GetClipCursor", (PROC)fake_GetClipCursor, (PROC*)&real_GetClipCursor, 0 },
            { "WindowFromPoint", (PROC)fake_WindowFromPoint, (PROC*)&real_WindowFromPoint, 0 },
            { "GetCursorInfo", (PROC)fake_GetCursorInfo, (PROC*)&real_GetCursorInfo, 0 },
            { "GetSystemMetrics", (PROC)fake_GetSystemMetrics, (PROC*)&real_GetSystemMetrics, 0 },
            { "SetWindowPos", (PROC)fake_SetWindowPos, (PROC*)&real_SetWindowPos, 0 },
            { "MoveWindow", (PROC)fake_MoveWindow, (PROC*)&real_MoveWindow, 0 },
            { "SendMessageA", (PROC)fake_SendMessageA, (PROC*)&real_SendMessageA, 0 },
            { "SetWindowLongA", (PROC)fake_SetWindowLongA, (PROC*)&real_SetWindowLongA, 0 },
            { "GetWindowLongA", (PROC)fake_GetWindowLongA, (PROC*)&real_GetWindowLongA, 0 },
            { "EnableWindow", (PROC)fake_EnableWindow, (PROC*)&real_EnableWindow, 0 },
            { "CreateWindowExA", (PROC)fake_CreateWindowExA, (PROC*)&real_CreateWindowExA, 0 },
            { "DestroyWindow", (PROC)fake_DestroyWindow, (PROC*)&real_DestroyWindow, 0 },
            { "MapWindowPoints", (PROC)fake_MapWindowPoints, (PROC*)&real_MapWindowPoints, 0 },
            { "ShowWindow", (PROC)fake_ShowWindow, (PROC*)&real_ShowWindow, 0 },
            { "GetTopWindow", (PROC)fake_GetTopWindow, (PROC*)&real_GetTopWindow, 0 },
            { "GetForegroundWindow", (PROC)fake_GetForegroundWindow, (PROC*)&real_GetForegroundWindow, 0 },
            { "PeekMessageA", (PROC)fake_PeekMessageA, (PROC*)&real_PeekMessageA, 0 },
            { "SetForegroundWindow", (PROC)fake_SetForegroundWindow, (PROC*)&real_SetForegroundWindow, 0 },
            { "SetWindowsHookExA", (PROC)fake_SetWindowsHookExA, (PROC*)&real_SetWindowsHookExA, 0 },
            { "", NULL, NULL, 0 }
        }
    },
    {
        "ole32.dll",
        {
            { "CoCreateInstance", (PROC)fake_CoCreateInstance, (PROC*)&real_CoCreateInstance, HOOK_SKIP_2 },
            { "", NULL, NULL, 0 }
        }
    },
        {
        "winmm.dll",
        {
            { "mciSendCommandA", (PROC)fake_mciSendCommandA, (PROC*)&real_mciSendCommandA, HOOK_SKIP_2 },
            { "", NULL, NULL, 0 }
        }
    },
    {
        "dinput.dll",
        {
            { "DirectInputCreateA", (PROC)fake_DirectInputCreateA, (PROC*)&real_DirectInputCreateA, HOOK_SKIP_2 },
            //{ "DirectInputCreateW", (PROC)fake_DirectInputCreateW, (PROC*)&real_DirectInputCreateW, HOOK_SKIP_2 },
            { "DirectInputCreateEx", (PROC)fake_DirectInputCreateEx, (PROC*)&real_DirectInputCreateEx, HOOK_SKIP_2 },
            { "", NULL, NULL, 0 }
        }
    },
    {
        "dinput8.dll",
        {
            { "DirectInput8Create", (PROC)fake_DirectInput8Create, (PROC*)&real_DirectInput8Create, HOOK_SKIP_2 },
            { "", NULL, NULL, 0 }
        }
    },
    {
        "gdi32.dll",
        {
            { "StretchBlt", (PROC)fake_StretchBlt, (PROC*)&real_StretchBlt, HOOK_SKIP_2 },
            { "SetDIBitsToDevice", (PROC)fake_SetDIBitsToDevice, (PROC*)&real_SetDIBitsToDevice, HOOK_SKIP_2 },
            { "StretchDIBits", (PROC)fake_StretchDIBits, (PROC*)&real_StretchDIBits, HOOK_SKIP_2 },
            { "GetDeviceCaps", (PROC)fake_GetDeviceCaps, (PROC*)&real_GetDeviceCaps, HOOK_LOCAL_ONLY },
            { "CreateFontA", (PROC)fake_CreateFontA, (PROC*)&real_CreateFontA, 0 },
            { "CreateFontIndirectA", (PROC)fake_CreateFontIndirectA, (PROC*)&real_CreateFontIndirectA, 0 },
            { "", NULL, NULL, 0 }
        }
    },
    {
        "kernel32.dll",
        {
            { "LoadLibraryA", (PROC)fake_LoadLibraryA, (PROC*)&real_LoadLibraryA, HOOK_SKIP_2 },
            { "LoadLibraryW", (PROC)fake_LoadLibraryW, (PROC*)&real_LoadLibraryW, HOOK_SKIP_2 },
            { "LoadLibraryExA", (PROC)fake_LoadLibraryExA, (PROC*)&real_LoadLibraryExA, HOOK_SKIP_2 },
            { "LoadLibraryExW", (PROC)fake_LoadLibraryExW, (PROC*)&real_LoadLibraryExW, HOOK_SKIP_2 },
            { "GetProcAddress", (PROC)fake_GetProcAddress, (PROC*)&real_GetProcAddress, HOOK_SKIP_2 },
            { "GetDiskFreeSpaceA", (PROC)fake_GetDiskFreeSpaceA, (PROC*)&real_GetDiskFreeSpaceA, HOOK_SKIP_2 },
            { "", NULL, NULL, 0 }
        }
    },
    {
        "",
        {
            { "", NULL, NULL, 0 }
        }
    }
};

void hook_patch_iat(HMODULE hmod, BOOL unhook, char* module_name, char* function_name, PROC new_function)
{
    HOOKLIST hooks[2];
    memset(&hooks, 0, sizeof(hooks));

    hooks[0].data[0].new_function = new_function;

    strncpy(hooks[0].module_name, module_name, sizeof(hooks[0].module_name) - 1);
    strncpy(hooks[0].data[0].function_name, function_name, sizeof(hooks[0].data[0].function_name) - 1);

    hook_patch_iat_list(hmod, unhook, (HOOKLIST*)&hooks, FALSE);
}

void hook_patch_obfuscated_iat_list(HMODULE hmod, BOOL unhook, HOOKLIST* hooks, BOOL is_local)
{
    if (!hmod || hmod == INVALID_HANDLE_VALUE || !hooks)
        return;

    __try
    {
        PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)hmod;
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
            return;

        PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((DWORD)dos_header + (DWORD)dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
            return;

        PIMAGE_IMPORT_DESCRIPTOR import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)dos_header +
            (DWORD)(nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

        if (import_desc == (PIMAGE_IMPORT_DESCRIPTOR)nt_headers)
            return;

        while (import_desc->FirstThunk)
        {
            for (int i = 0; hooks[i].module_name[0]; i++)
            {
                char* imp_module_name = (char*)((DWORD)dos_header + (DWORD)(import_desc->Name));

                if (_stricmp(imp_module_name, hooks[i].module_name) == 0)
                {
                    HMODULE cur_mod = GetModuleHandle(hooks[i].module_name);

                    PIMAGE_THUNK_DATA first_thunk =
                        (PIMAGE_THUNK_DATA)((DWORD)dos_header + (DWORD)import_desc->FirstThunk);

                    PIMAGE_THUNK_DATA original_first_thunk =
                        (PIMAGE_THUNK_DATA)((DWORD)dos_header + (DWORD)import_desc->OriginalFirstThunk);

                    while (first_thunk->u1.Function)
                    {
                        for (int x = 0; hooks[i].data[x].function_name[0]; x++)
                        {
                            DWORD org_function =
                                (DWORD)real_GetProcAddress(
                                    cur_mod,
                                    hooks[i].data[x].function_name);

                            if (!hooks[i].data[x].new_function || !org_function)
                                continue;

                            if (!is_local && (hooks[i].data[x].flags & HOOK_LOCAL_ONLY))
                                continue;

                            if (unhook)
                            {
                                if (first_thunk->u1.Function == (DWORD)hooks[i].data[x].new_function)
                                {
                                    DWORD op;

                                    if (VirtualProtect(
                                        &first_thunk->u1.Function, 
                                        sizeof(DWORD), 
                                        PAGE_READWRITE, 
                                        &op))
                                    {
                                        first_thunk->u1.Function = org_function;

                                        VirtualProtect(&first_thunk->u1.Function, sizeof(DWORD), op, &op);
                                    }

                                    break;
                                }
                            }
                            else
                            {
                                if (first_thunk->u1.Function == org_function)
                                {
                                    DWORD op;

                                    if (VirtualProtect(
                                        &first_thunk->u1.Function, 
                                        sizeof(DWORD), 
                                        PAGE_READWRITE, 
                                        &op))
                                    {
                                        first_thunk->u1.Function = (DWORD)hooks[i].data[x].new_function;

                                        VirtualProtect(&first_thunk->u1.Function, sizeof(DWORD), op, &op);
                                    }

                                    break;
                                }
                            }
                        }

                        first_thunk++;
                        original_first_thunk++;
                    }
                }
            }

            import_desc++;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void hook_patch_iat_list(HMODULE hmod, BOOL unhook, HOOKLIST* hooks, BOOL is_local)
{
    hook_patch_obfuscated_iat_list(hmod, unhook, hooks, is_local);

    if (!hmod || hmod == INVALID_HANDLE_VALUE || !hooks)
        return;

    __try
    {
        PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)hmod;
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
            return;

        PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((DWORD)dos_header + (DWORD)dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
            return;

        PIMAGE_IMPORT_DESCRIPTOR import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)dos_header +
            (DWORD)(nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

        if (import_desc == (PIMAGE_IMPORT_DESCRIPTOR)nt_headers)
            return;

        while (import_desc->FirstThunk)
        {
            for (int i = 0; hooks[i].module_name[0]; i++)
            {
                char* imp_module_name = (char*)((DWORD)dos_header + (DWORD)(import_desc->Name));

                if (_stricmp(imp_module_name, hooks[i].module_name) == 0)
                {
                    PIMAGE_THUNK_DATA first_thunk =
                        (PIMAGE_THUNK_DATA)((DWORD)dos_header + (DWORD)import_desc->FirstThunk);

                    PIMAGE_THUNK_DATA original_first_thunk =
                        (PIMAGE_THUNK_DATA)((DWORD)dos_header + (DWORD)import_desc->OriginalFirstThunk);

                    while (first_thunk->u1.Function && original_first_thunk->u1.AddressOfData)
                    {
                        PIMAGE_IMPORT_BY_NAME import =
                            (PIMAGE_IMPORT_BY_NAME)((DWORD)dos_header + original_first_thunk->u1.AddressOfData);

                        if ((original_first_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)
                        {
                            for (int x = 0; hooks[i].data[x].function_name[0]; x++)
                            {
                                if (!unhook && !hooks[i].data[x].new_function)
                                    continue;

                                if (!is_local && (hooks[i].data[x].flags & HOOK_LOCAL_ONLY))
                                    continue;

                                if (_stricmp((const char*)import->Name, hooks[i].data[x].function_name) == 0)
                                {
                                    DWORD op;

                                    if (VirtualProtect(
                                        &first_thunk->u1.Function, 
                                        sizeof(DWORD), 
                                        PAGE_READWRITE, 
                                        &op))
                                    {
                                        if (unhook)
                                        {
                                            DWORD org =
                                                (DWORD)real_GetProcAddress(
                                                    GetModuleHandle(hooks[i].module_name),
                                                    hooks[i].data[x].function_name);

                                            if (org && first_thunk->u1.Function != org)
                                            {
                                                first_thunk->u1.Function = org;
                                            }
                                        }
                                        else
                                        {
                                            if (first_thunk->u1.Function != (DWORD)hooks[i].data[x].new_function)
                                                first_thunk->u1.Function = (DWORD)hooks[i].data[x].new_function;
                                        }

                                        VirtualProtect(&first_thunk->u1.Function, sizeof(DWORD), op, &op);
                                    }

                                    break;
                                }
                            }
                        }

                        first_thunk++;
                        original_first_thunk++;
                    }
                }
            }

            import_desc++;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

BOOL hook_got_ddraw_import()
{
    __try
    {
        HMODULE hmod = GetModuleHandleA(NULL);

        PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)hmod;
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
            return FALSE;

        PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((DWORD)dos_header + (DWORD)dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
            return FALSE;

        PIMAGE_IMPORT_DESCRIPTOR import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)dos_header +
            (DWORD)(nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

        if (import_desc == (PIMAGE_IMPORT_DESCRIPTOR)nt_headers)
            return FALSE;

        while (import_desc->FirstThunk)
        {
            char* imp_module_name = (char*)((DWORD)dos_header + (DWORD)(import_desc->Name));

            if (_stricmp(imp_module_name, "ddraw.dll") == 0)
            {
                return TRUE;
            }

            import_desc++;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return FALSE;
}

void hook_create(HOOKLIST* hooks, BOOL initial_hook)
{
#ifdef _MSC_VER
    if ((g_config.hook == 2) && initial_hook)
    {
        for (int i = 0; hooks[i].module_name[0]; i++)
        {
            for (int x = 0; hooks[i].data[x].function_name[0]; x++)
            {
                if ((hooks[i].data[x].flags & HOOK_SKIP_2))
                    continue;

                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourAttach((PVOID*)hooks[i].data[x].function, (PVOID)hooks[i].data[x].new_function);
                DetourTransactionCommit();
            }
        }
    }
#endif

    if (g_config.hook == 3 || g_config.hook == 4)
    {
        char game_exe_path[MAX_PATH] = { 0 };
        char game_dir[MAX_PATH] = { 0 };

        if (GetModuleFileNameA(NULL, game_exe_path, MAX_PATH))
        {
            _splitpath(game_exe_path, NULL, game_dir, NULL, NULL);

            char mod_path[MAX_PATH] = { 0 };
            char mod_dir[MAX_PATH] = { 0 };
            char mod_filename[MAX_PATH] = { 0 };
            HMODULE hmod = NULL;
            HANDLE process = NULL;

#ifndef _MSC_VER
            HMODULE mods[512];
            memset(mods, 0, sizeof(mods));
            process = GetCurrentProcess();
            EnumProcessModules(process, mods, sizeof(mods) - sizeof(mods[0]), NULL);
            for (int i = 0; i < sizeof(mods) / sizeof(mods[0]) && (hmod = mods[i]); i++)
#else
            while (hmod = DetourEnumerateModules(hmod))
#endif
            {
                if (hmod == g_ddraw_module)
                    continue;

                if (GetModuleFileNameA(hmod, mod_path, MAX_PATH))
                {
                    if (initial_hook)
                    {
                        TRACE("Module %s = %p\n", mod_path, hmod);
                    }

                    _splitpath(mod_path, NULL, mod_dir, mod_filename, NULL);

                    /* Don't hook reshade/swiftshader/mesa3d */
                    if (_strcmpi(mod_filename, "opengl32") == 0 ||
                        _strcmpi(mod_filename, "d3d9") == 0 ||
                        _strcmpi(mod_filename, "Shw32") == 0)
                        continue;

                    BOOL is_local = _strnicmp(game_dir, mod_dir, strlen(game_dir)) == 0;
                    BOOL is_wine = real_GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != 0;

                    BOOL wine_hook = is_wine && _strcmpi(mod_filename, "mciavi32") == 0;

                    if (is_local ||
                        wine_hook ||
                        _strcmpi(mod_filename, "MSVFW32") == 0 ||
                        _strcmpi(mod_filename, "quartz") == 0 ||
                        _strcmpi(mod_filename, "winmm") == 0)
                    {
                        hook_patch_iat_list(hmod, FALSE, hooks, is_local);
                    }
                }
            }

            if (process)
                CloseHandle(process);
        }
    }

    if (g_config.hook == 1)
    {
        hook_patch_iat_list(GetModuleHandle(NULL), FALSE, hooks, TRUE);
    }
}

void hook_revert(HOOKLIST* hooks)
{
#ifdef _MSC_VER
    if (g_config.hook == 2)
    {
        for (int i = 0; hooks[i].module_name[0]; i++)
        {
            for (int x = 0; hooks[i].data[x].function_name[0]; x++)
            {
                if ((hooks[i].data[x].flags & HOOK_SKIP_2))
                    continue;

                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourDetach((PVOID*)hooks[i].data[x].function, (PVOID)hooks[i].data[x].new_function);
                DetourTransactionCommit();
            }
        }
    }
#endif

    if (g_config.hook == 3 || g_config.hook == 4)
    {
        char game_exe_path[MAX_PATH] = { 0 };
        char game_dir[MAX_PATH] = { 0 };

        if (GetModuleFileNameA(NULL, game_exe_path, MAX_PATH))
        {
            _splitpath(game_exe_path, NULL, game_dir, NULL, NULL);

            char mod_path[MAX_PATH] = { 0 };
            char mod_dir[MAX_PATH] = { 0 };
            char mod_filename[MAX_PATH] = { 0 };
            HMODULE hmod = NULL;
            HANDLE process = NULL;

#ifndef _MSC_VER
            HMODULE mods[512];
            memset(mods, 0, sizeof(mods));
            process = GetCurrentProcess();
            EnumProcessModules(process, mods, sizeof(mods) - sizeof(mods[0]), NULL);
            for (int i = 0; i < sizeof(mods) / sizeof(mods[0]) && (hmod = mods[i]); i++)
#else
            while (hmod = DetourEnumerateModules(hmod))
#endif
            {
                if (hmod == g_ddraw_module)
                    continue;

                if (GetModuleFileNameA(hmod, mod_path, MAX_PATH))
                {
                    _splitpath(mod_path, NULL, mod_dir, mod_filename, NULL);

                    BOOL is_local = _strnicmp(game_dir, mod_dir, strlen(game_dir)) == 0;
                    BOOL is_wine = real_GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != 0;

                    BOOL wine_hook = is_wine && _strcmpi(mod_filename, "mciavi32") == 0;

                    if (is_local ||
                        wine_hook ||
                        _strcmpi(mod_filename, "MSVFW32") == 0 ||
                        _strcmpi(mod_filename, "quartz") == 0 ||
                        _strcmpi(mod_filename, "winmm") == 0)
                    {
                        hook_patch_iat_list(hmod, TRUE, hooks, is_local);
                    }
                }
            }

            if (process)
                CloseHandle(process);
        }
    }

    if (g_config.hook == 1)
    {
        hook_patch_iat_list(GetModuleHandle(NULL), TRUE, hooks, TRUE);
    }
}

void hook_init(BOOL initial_hook)
{
    if (initial_hook)
    {
        if (g_config.hook == 4 && hook_got_ddraw_import())
        {
            /* Switch to 3 if we can be sure that ddraw.dll will not be unloaded from the process */
            g_config.hook = 3;
        }
    }

    if (!g_hook_active || g_config.hook == 3 || g_config.hook == 4)
    {
#if defined(_DEBUG) && defined(_MSC_VER)
        if (initial_hook)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach((PVOID*)&real_SetUnhandledExceptionFilter, (PVOID)fake_SetUnhandledExceptionFilter);
            DetourTransactionCommit();
        }
#endif

        if (initial_hook)
        {
            hook_patch_iat(GetModuleHandle("AcGenral"), FALSE, "user32.dll", "SetWindowsHookExA", (PROC)fake_SetWindowsHookExA);
        }

        hook_create((HOOKLIST*)&g_hook_hooklist, initial_hook);

        g_hook_active = TRUE;
    }
}

void hook_exit()
{
    if (g_hook_active)
    {
        g_hook_active = FALSE;

        hook_revert((HOOKLIST*)&g_hook_hooklist);

#if defined(_DEBUG) && defined(_MSC_VER)
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach((PVOID*)&real_SetUnhandledExceptionFilter, (PVOID)fake_SetUnhandledExceptionFilter);
        DetourTransactionCommit();

        real_SetUnhandledExceptionFilter(g_dbg_exception_filter);
#endif

        hook_patch_iat(GetModuleHandle("AcGenral"), TRUE, "user32.dll", "SetWindowsHookExA", (PROC)fake_SetWindowsHookExA);
    }
}
