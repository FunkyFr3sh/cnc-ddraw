#include <windows.h>
#include "debug.h"
#include "hook.h"
#include "dd.h"
#include "utils.h"
#include "config.h"
#include "keyboard.h"
#include "mouse.h"
#include "screenshot.h"


HHOOK g_keyboard_hook;

void keyboard_hook_init()
{
    if (g_keyboard_hook && UnhookWindowsHookEx(g_keyboard_hook))
    {
        g_keyboard_hook = NULL;
    }  

    if (!g_ddraw.gui_thread_id)
        return;

    g_keyboard_hook = 
        real_SetWindowsHookExA(
            WH_KEYBOARD, 
            keyboard_hook_proc, 
            NULL, 
            g_ddraw.gui_thread_id);
}

void keyboard_hook_exit()
{
    if (g_keyboard_hook)
    {
        UnhookWindowsHookEx(g_keyboard_hook);
    }
}

LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wParam, LPARAM lParam)
{
    /* PM_NOREMOVE may report the same keystroke repeatedly. Act only on removal. */
    if (code != HC_ACTION || !wParam)
        return CallNextHookEx(g_keyboard_hook, code, wParam, lParam);

    BOOL alt_down = !!(lParam & (1 << 29));
    BOOL key_down = !(lParam & (1 << 31));
    BOOL key_released = !!(lParam & (1 << 31));
    BOOL key_triggered = !(lParam & (1 << 30));

    //TRACE("kbhook code=%u, wParam=%u, triggered=%u, released=%u, alt_down=%u\n", code, wParam, key_triggered, key_released, alt_down);

    if (wParam == VK_MENU && (key_released || key_triggered)) /* Fix for alt key being stuck on alt+tab in some games */
    {
        g_ddraw.alt_key_down = alt_down;
    }

    if (wParam == g_config.hotkeys.toggle_fullscreen && alt_down && key_down)
    {
        if (key_triggered)
            util_toggle_fullscreen();

        return 1;
    }

    if (wParam == g_config.hotkeys.toggle_fullscreen2)
    {
        if (key_triggered)
            util_toggle_fullscreen();

        return 1;
    }

    if (wParam == g_config.hotkeys.toggle_maximize && alt_down && key_down)
    {
        if (key_triggered)
            util_toggle_maximize();

        return 1;
    }

    if (wParam == g_config.hotkeys.toggle_maximize2)
    {
        if (key_triggered)
            util_toggle_maximize();

        return 1;
    }

    if (wParam == g_config.hotkeys.screenshot)
    {
        // VK_SNAPSHOT does not have a KEYDOWN event...
        if (g_config.hotkeys.screenshot == VK_SNAPSHOT)
        {
            if (key_released)
            {
                ss_take_screenshot(g_ddraw.primary);
                return 1;
            }
        }
        else if (key_triggered)
        {
            ss_take_screenshot(g_ddraw.primary);
            return 1;
        }
    }

    if (wParam == g_config.hotkeys.unlock_cursor1 || wParam == VK_CONTROL)
    {
        if ((real_GetAsyncKeyState(VK_CONTROL) & 0x8000) && 
            (real_GetAsyncKeyState(g_config.hotkeys.unlock_cursor1) & 0x8000))
        {
            mouse_unlock();

            if (key_down)
                return 1;
        }
    }

    if (wParam == g_config.hotkeys.unlock_cursor2 || wParam == VK_MENU || wParam == VK_CONTROL)
    {
        if ((real_GetAsyncKeyState(VK_RMENU) & 0x8000) && 
            (real_GetAsyncKeyState(g_config.hotkeys.unlock_cursor2) & 0x8000))
        {
            mouse_unlock();

            if (key_down)
                return 1;
        }
    }

    return CallNextHookEx(g_keyboard_hook, code, wParam, lParam);
}
