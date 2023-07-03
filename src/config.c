#include <windows.h>
#include <stdio.h>
#include <d3d9.h>
#include "fps_limiter.h"
#include "config.h"
#include "dd.h"
#include "render_d3d9.h"
#include "render_gdi.h"
#include "render_ogl.h"
#include "hook.h"
#include "debug.h"

#define FILE_EXISTS(a) (GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES)

static void cfg_init();
static void cfg_create_ini();

CNCDDRAWCONFIG g_config =
    { .window_rect = {.left = -32000, .top = -32000, .right = 0, .bottom = 0 }, .window_state = -1, .borderless_state = -1 };

void cfg_load()
{
    char tmp[256];

    cfg_init();

    /* load settings from ini */
    g_ddraw->windowed = cfg_get_bool("windowed", FALSE);
    g_ddraw->border = cfg_get_bool("border", TRUE);
    g_ddraw->boxing = cfg_get_bool("boxing", FALSE);
    g_ddraw->maintas = cfg_get_bool("maintas", FALSE);
    g_ddraw->adjmouse = 
        cfg_get_bool("adjmouse", TRUE) || 
        !cfg_get_bool("handlemouse", TRUE) || 
        FILE_EXISTS(".\\plugin\\HardwareCursor.w2p");

    g_ddraw->devmode = cfg_get_bool("devmode", FALSE);
    g_ddraw->vsync = cfg_get_bool("vsync", FALSE);
    g_ddraw->noactivateapp = cfg_get_bool("noactivateapp", FALSE);
    g_ddraw->vhack = cfg_get_bool("vhack", FALSE);
    g_ddraw->accurate_timers = cfg_get_bool("accuratetimers", FALSE);
    g_ddraw->resizable = cfg_get_bool("resizable", TRUE);
    g_ddraw->toggle_borderless = cfg_get_bool("toggle_borderless", FALSE);
    g_ddraw->nonexclusive = cfg_get_bool("nonexclusive", FALSE);
    g_ddraw->fixpitch = cfg_get_bool("fixpitch", FALSE);
    g_ddraw->fixchilds = cfg_get_int("fixchilds", FIX_CHILDS_DETECT_PAINT);
    g_ddraw->fixnotresponding = cfg_get_bool("fixnotresponding", FALSE);
    g_ddraw->locktopleft = cfg_get_bool("locktopleft", FALSE);
    g_ddraw->releasealt = cfg_get_bool("releasealt", FALSE);
    g_ddraw->d3d9linear = cfg_get_bool("d3d9linear", TRUE);
    g_ddraw->gdilinear = cfg_get_bool("gdilinear", FALSE);
    g_ddraw->resolutions = cfg_get_int("resolutions", RESLIST_NORMAL);

    if (g_ddraw->locktopleft)
        g_ddraw->adjmouse = FALSE;

    g_ddraw->armadahack = cfg_get_bool("armadahack", FALSE);
    g_ddraw->tshack = cfg_get_bool("tshack", FALSE);

    g_ddraw->hotkeys.toggle_fullscreen = cfg_get_int("keytogglefullscreen", VK_RETURN);
    g_ddraw->hotkeys.toggle_maximize = cfg_get_int("keytogglemaximize", VK_NEXT);
    g_ddraw->hotkeys.unlock_cursor1 = cfg_get_int("keyunlockcursor1", VK_TAB);
    g_ddraw->hotkeys.unlock_cursor2 = cfg_get_int("keyunlockcursor2", VK_RCONTROL);
    g_ddraw->hotkeys.screenshot = cfg_get_int("keyscreenshot", VK_SNAPSHOT);

    g_config.window_rect.right = cfg_get_int("width", 0);
    g_config.window_rect.bottom = cfg_get_int("height", 0);
    g_config.window_rect.left = cfg_get_int("posX", -32000);
    g_config.window_rect.top = cfg_get_int("posY", -32000);

    g_config.save_settings = cfg_get_int("savesettings", 1);

    g_hook_method = cfg_get_int("hook", 4);
    g_hook_dinput = cfg_get_bool("dinputhook", FALSE);

    g_ddraw->render.maxfps = cfg_get_int("maxfps", -1);
    g_ddraw->render.minfps = cfg_get_int("minfps", 0);

    if (g_ddraw->render.minfps > 1000)
    {
        g_ddraw->render.minfps = 1000;
    }

    if (g_ddraw->render.minfps > 0)
    {
        g_ddraw->render.minfps_tick_len = (DWORD)(1000.0f / g_ddraw->render.minfps);
    }

    /* can't fully set it up here due to missing g_ddraw->mode.dmDisplayFrequency  */
    if (g_ddraw->accurate_timers || g_ddraw->vsync)
        g_fpsl.htimer = CreateWaitableTimer(NULL, TRUE, NULL);

    g_ddraw->maxgameticks = cfg_get_int("maxgameticks", 0);

    if (g_ddraw->maxgameticks > 0 && g_ddraw->maxgameticks <= 1000)
    {
        if (g_ddraw->accurate_timers)
            g_ddraw->ticks_limiter.htimer = CreateWaitableTimer(NULL, TRUE, NULL);

        float len = 1000.0f / g_ddraw->maxgameticks;
        g_ddraw->ticks_limiter.tick_length_ns = (LONGLONG)(len * 10000);
        g_ddraw->ticks_limiter.tick_length = (DWORD)(len + 0.5f);
    }

    if (g_ddraw->maxgameticks >= 0 || g_ddraw->maxgameticks == -2)
    {
        /* always using 60 fps for flip...  */
        if (g_ddraw->accurate_timers)
            g_ddraw->flip_limiter.htimer = CreateWaitableTimer(NULL, TRUE, NULL);

        float flip_len = 1000.0f / 60;
        g_ddraw->flip_limiter.tick_length_ns = (LONGLONG)(flip_len * 10000);
        g_ddraw->flip_limiter.tick_length = (DWORD)(flip_len + 0.5f);
    }

    g_ddraw->fullscreen = cfg_get_bool("fullscreen", FALSE);

    if (cfg_get_bool("singlecpu", TRUE))
    {
        SetProcessAffinityMask(GetCurrentProcess(), 1);
    }
    else
    {
        DWORD system_affinity;
        DWORD proc_affinity;
        HANDLE proc = GetCurrentProcess();

        if (GetProcessAffinityMask(proc, &proc_affinity, &system_affinity))
            SetProcessAffinityMask(proc, system_affinity);
    }

    g_ddraw->render.bpp = cfg_get_int("bpp", 0);

    if (g_ddraw->render.bpp != 16 && g_ddraw->render.bpp != 24 && g_ddraw->render.bpp != 32)
    {
        g_ddraw->render.bpp = 0;
    }

    /* to do: read .glslp config file instead of the shader and apply the correct settings  */
    cfg_get_string("shader", "", g_ddraw->shader, sizeof(g_ddraw->shader));

    cfg_get_string("renderer", "auto", tmp, sizeof(tmp));
    TRACE("     Using %s renderer\n", tmp);

    if (tolower(tmp[0]) == 's' || tolower(tmp[0]) == 'g') /* gdi */
    {
        g_ddraw->renderer = gdi_render_main;
    }
    else if (tolower(tmp[0]) == 'd') /* direct3d9 */
    {
        g_ddraw->renderer = d3d9_render_main;

        if (!g_ddraw->windowed)
        {
            g_ddraw->fullscreen = TRUE;
            g_ddraw->windowed = TRUE;
            g_config.window_rect.left = g_config.window_rect.top = -32000;
        }
    }
    else if (tolower(tmp[0]) == 'o') /* opengl */
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

            if (!g_ddraw->windowed)
            {
                g_ddraw->fullscreen = TRUE;
                g_ddraw->windowed = TRUE;
                g_config.window_rect.left = g_config.window_rect.top = -32000;
            }
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
}

void cfg_save()
{
    if (!g_config.save_settings)
        return;

    char buf[16];
    char* section = g_config.save_settings == 1 ? "ddraw" : g_config.process_file_name;

    if (g_config.window_rect.right)
    {
        sprintf(buf, "%ld", g_config.window_rect.right);
        WritePrivateProfileString(section, "width", buf, g_config.ini_path);
    }

    if (g_config.window_rect.bottom)
    {
        sprintf(buf, "%ld", g_config.window_rect.bottom);
        WritePrivateProfileString(section, "height", buf, g_config.ini_path);
    }

    if (g_config.window_rect.left != -32000)
    {
        sprintf(buf, "%ld", g_config.window_rect.left);
        WritePrivateProfileString(section, "posX", buf, g_config.ini_path);
    }

    if (g_config.window_rect.top != -32000)
    {
        sprintf(buf, "%ld", g_config.window_rect.top);
        WritePrivateProfileString(section, "posY", buf, g_config.ini_path);
    }

    if (g_config.window_state != -1)
    {
        WritePrivateProfileString(section, "windowed", g_config.window_state ? "true" : "false", g_config.ini_path);
    }

    if (g_config.borderless_state != -1)
    {
        WritePrivateProfileString(section, "fullscreen", g_config.borderless_state ? "true" : "false", g_config.ini_path);
    }
}

static void cfg_create_ini()
{
    FILE* fh = fopen(g_config.ini_path, "w");
    if (fh)
    {
        fputs(
            "; cnc-ddraw - https://github.com/FunkyFr3sh/cnc-ddraw\n"
            "\n"
            "[ddraw]\n"
            "; ### Optional settings ###\n"
            "; Use the following settings to adjust the look and feel to your liking\n"
            "\n"
            "\n"
            "; Stretch to custom resolution, 0 = defaults to the size game requests\n"
            "width=0\n"
            "height=0\n"
            "\n"
            "; Override the width/height settings shown above and always stretch to fullscreen\n"
            "; Note: Can be combined with 'windowed=true' to get windowed-fullscreen aka borderless mode\n"
            "fullscreen=false\n"
            "\n"
            "; Run in windowed mode rather than going fullscreen\n"
            "windowed=false\n"
            "\n"
            "; Maintain aspect ratio\n"
            "maintas=true\n"
            "\n"
            "; Windowboxing / Integer Scaling\n"
            "boxing=false\n"
            "\n"
            "; Real rendering rate, -1 = screen rate, 0 = unlimited, n = cap\n"
            "; Note: Does not have an impact on the game speed, to limit your game speed use 'maxgameticks='\n"
            "maxfps=-1\n"
            "\n"
            "; Vertical synchronization, enable if you get tearing - (Requires 'renderer=auto/opengl/direct3d9')\n"
            "; Note: vsync=true can fix tearing but it will cause input lag\n"
            "vsync=false\n"
            "\n"
            "; Automatic mouse sensitivity scaling\n"
            "; Note: Only works if stretching is enabled. Sensitivity will be adjusted according to the size of the window\n"
            "adjmouse=false\n"
            "\n"
            "; Preliminary libretro shader support - (Requires 'renderer=opengl') https://github.com/libretro/glsl-shaders\n"
            "; 2x scaling example: https://imgur.com/a/kxsM1oY - 4x scaling example: https://imgur.com/a/wjrhpFV\n"
            "shader=Shaders\\interpolation\\catmull-rom-bilinear.glsl\n"
            "\n"
            "; Window position, -32000 = center to screen\n"
            "posX=-32000\n"
            "posY=-32000\n"
            "\n"
            "; Renderer, possible values: auto, opengl, gdi, direct3d9 (auto = try direct3d9/opengl, fallback = gdi)\n"
            "renderer=opengl\n"
            "\n"
            "; Developer mode (don't lock the cursor)\n"
            "devmode=false\n"
            "\n"
            "; Show window borders in windowed mode\n"
            "border=true\n"
            "\n"
            "; Save window position/size/state on game exit and restore it automatically on next game start\n"
            "; Possible values: 0 = disabled, 1 = save to global 'ddraw' section, 2 = save to game specific section\n"
            "savesettings=1\n"
            "\n"
            "; Should the window be resizable by the user in windowed mode?\n"
            "resizable=true\n"
            "\n"
            "; Enable linear (D3DTEXF_LINEAR) upscaling filter for the direct3d9 renderer\n"
            "d3d9linear=true\n"
            "\n"
            "; Enable upscale hack for high resolution patches (Supports C&C1, Red Alert 1 and KKND Xtreme)\n"
            "vhack=false\n"
            "\n"
            "; cnc-ddraw config program language, possible values: auto, english, chinese, german, spanish, russian, hungarian, french\n"
            "configlang=auto\n"
            "\n"
            "; Switch between windowed/borderless modes with alt+enter rather than windowed/fullscreen modes\n"
            "toggle_borderless=false\n"
            "\n"
            "\n"
            "\n"
            "; ### Compatibility settings ###\n"
            "; Use the following settings in case there are any issues with the game\n"
            "\n"
            "\n"
            "; Hide WM_ACTIVATEAPP and WM_NCACTIVATE messages to prevent problems on alt+tab\n"
            "noactivateapp=false\n"
            "\n"
            "; Max game ticks per second, possible values: -1 = disabled, -2 = refresh rate, 0 = emulate 60hz vblank, 1-1000 = custom game speed\n"
            "; Note: Can be used to slow down a too fast running game, fix flickering or too fast animations\n"
            "; Note: Usually one of the following values will work: 60 / 30 / 25 / 20 / 15 (lower value = slower game speed)\n"
            "maxgameticks=0\n"
            "\n"
            "; Windows API Hooking, Possible values: 0 = disabled, 1 = IAT Hooking, 2 = Microsoft Detours, 3 = IAT+Detours Hooking (All Modules), 4 = IAT Hooking (All Modules)\n"
            "; Note: Change this value if windowed mode or upscaling isn't working properly\n"
            "; Note: 'hook=2' will usually work for problematic games, but 'hook=2' should be combined with renderer=gdi\n"
            "hook=4\n"
            "\n"
            "; Force minimum FPS, possible values: 0 = disabled, -1 = use 'maxfps=' value, -2 = same as -1 but force full redraw, 1-1000 = custom FPS\n"
            "; Note: Set this to a low value such as 5 or 10 if some parts of the game are not being displayed (e.g. menus or loading screens)\n"
            "minfps=0\n"
            "\n"
            "; Disable fullscreen-exclusive mode for the direct3d9/opengl renderers\n"
            "; Note: Can be used in case some GUI elements like buttons/textboxes/videos/etc.. are invisible\n"
            "nonexclusive=false\n"
            "\n"
            "; Fixes issues where the pitch of a surface is not a multiple of 4\n"
            "; Note: Enable this if some parts of the screen are being displayed diagonally\n"
            "fixpitch=false\n"
            "\n"
            "; Force CPU0 affinity, avoids crashes/freezing, *might* have a performance impact\n"
            "singlecpu=true\n"
            "\n"
            "; Available resolutions, possible values: 0 = Small list, 1 = Very small list, 2 = Full list\n"
            "; Note: Set this to 2 if your chosen resolution is not working, set to 1 if the game is crashing\n"
            "resolutions=0\n"
            "\n"
            "; Child window handling, possible values: 0 = Disabled, 1 = Display top left, 2 = Display top left + repaint, 3 = Hide\n"
            "; Note: Disables upscaling if a child window was detected\n"
            "fixchilds=2\n"
            "\n"
            "\n"
            "\n"
            "; ### Hotkeys ###\n"
            "; Use the following settings to configure your hotkeys, 0x00 = disabled\n"
            "; Virtual-Key Codes: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes\n"
            "\n"
            "\n"
            "; Switch between windowed and fullscreen mode = [Alt] + ???\n"
            "keytogglefullscreen=0x0D\n"
            "\n"
            "; Maximize window without frame = [Alt] + ???\n"
            "keytogglemaximize=0x22\n"
            "\n"
            "; Unlock cursor 1 = [Ctrl] + ???\n"
            "keyunlockcursor1=0x09\n"
            "\n"
            "; Unlock cursor 2 = [Right Alt] + ???\n"
            "keyunlockcursor2=0xA3\n"
            "\n"
            "; Screenshot\n"
            "keyscreenshot=0x2C\n"
            "\n"
            "\n"
            "\n"
            "; ### Game specific settings ###\n"
            "; The following settings override all settings shown above, section name = executable name\n"
            "\n"
            "\n"
            "; Atomic Bomberman\n"
            "[BM]\n"
            "maxgameticks=60\n"
            "\n"
            "; Age of Empires\n"
            "[empires]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "resolutions=2\n"
            "\n"
            "; Age of Empires: The Rise of Rome\n"
            "[empiresx]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "resolutions=2\n"
            "\n"
            "; Age of Empires II\n"
            "[EMPIRES2]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "\n"
            "; Age of Empires II: The Conquerors\n"
            "[age2_x1]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "\n"
            "; American Conquest\n"
            "[DMCR]\n"
            "minfps=-2\n"
            "\n"
            "; Age of Wonders\n"
            "[AoWSM]\n"
            "windowed=true\n"
            "fullscreen=false\n"
            "renderer=gdi\n"
            "hook=2\n"
            "\n"
            "; Age of Wonders 2\n"
            "[AoW2]\n"
            "windowed=true\n"
            "fullscreen=false\n"
            "renderer=gdi\n"
            "hook=2\n"
            "\n"
            "; Anstoss 3\n"
            "[anstoss3]\n"
            "renderer=gdi\n"
            "adjmouse=true\n"
            "\n"
            "; Anno 1602\n"
            "[1602]\n"
            "adjmouse=true\n"
            "\n"
            "; Alien Nations\n"
            "[AN]\n"
            "adjmouse=true\n"
            "\n"
            "; Amerzone\n"
            "[AMERZONE]\n"
            "renderer=opengl\n"
            "\n"
            "; Atlantis\n"
            "[ATLANTIS]\n"
            "renderer=opengl\n"
            "maxgameticks=60\n"
            "\n"
            "; Blade & Sword\n"
            "[comeon]\n"
            "fixchilds=3\n"
            "fixpitch=true\n"
            "\n"
            "; Blood II - The Chosen / Shogo - Mobile Armor Division\n"
            "[Client]\n"
            "checkfile=.\\SOUND.REZ\n"
            "noactivateapp=true\n"
            "\n"
            "; Casino Empire\n"
            "[CasinoEmpire]\n"
            "hook=2\n"
            "\n"
            "; Carmageddon\n"
            "[CARMA95]\n"
            "renderer=opengl\n"
            "noactivateapp=true\n"
            "\n"
            "; Carmageddon\n"
            "[CARM95]\n"
            "renderer=opengl\n"
            "noactivateapp=true\n"
            "\n"
            "; Carmageddon 2\n"
            "[Carma2_SW]\n"
            "renderer=opengl\n"
            "noactivateapp=true\n"
            "\n"
            "; Captain Claw\n"
            "[claw]\n"
            "noactivateapp=true\n"
            "\n"
            "; Command & Conquer Gold - CnCNet\n"
            "[cnc95]\n"
            "maxfps=125\n"
            "\n"
            "; Command & Conquer Gold\n"
            "[C&C95]\n"
            "maxgameticks=120\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "\n"
            "; Command & Conquer: Red Alert - CnCNet\n"
            "[ra95-spawn]\n"
            "maxfps=125\n"
            "\n"
            "; Command & Conquer: Red Alert\n"
            "[ra95]\n"
            "maxgameticks=120\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "\n"
            "; Command & Conquer: Red Alert\n"
            "[ra95_Mod-Launcher]\n"
            "maxgameticks=120\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "\n"
            "; Command & Conquer: Red Alert\n"
            "[ra95p]\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "\n"
            "; Command & Conquer: Tiberian Sun / Command & Conquer: Red Alert 2\n"
            "[game]\n"
            "checkfile=.\\blowfish.dll\n"
            "tshack=true\n"
            "noactivateapp=true\n"
            "adjmouse=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Tiberian Sun Demo\n"
            "[SUN]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "adjmouse=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Tiberian Sun - CnCNet\n"
            "[ts-spawn]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "adjmouse=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Red Alert 2 - XWIS\n"
            "[ra2]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Red Alert 2 - XWIS\n"
            "[Red Alert 2]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Red Alert 2: Yuri's Revenge\n"
            "[gamemd]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Red Alert 2: Yuri's Revenge - ?ModExe?\n"
            "[ra2md]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Red Alert 2: Yuri's Revenge - CnCNet\n"
            "[gamemd-spawn]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Command & Conquer: Red Alert 2: Yuri's Revenge - XWIS\n"
            "[Yuri's Revenge]\n"
            "noactivateapp=true\n"
            "tshack=true\n"
            "maxfps=60\n"
            "minfps=-1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Caesar III\n"
            "[c3]\n"
            "nonexclusive=true\n"
            "adjmouse=true\n"
            "\n"
            "; Chris Sawyer's Locomotion\n"
            "[LOCO]\n"
            "adjmouse=true\n"
            "\n"
            "; Cultures 2\n"
            "[Cultures2]\n"
            "adjmouse=true\n"
            "\n"
            "; Cultures 2 MP\n"
            "[Cultures2MP]\n"
            "adjmouse=true\n"
            "\n"
            "; Close Combat 2: A Bridge Too Far\n"
            "[cc2]\n"
            "adjmouse=true\n"
            "fixpitch=true\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Close Combat 3: The Russian Front\n"
            "[cc3]\n"
            "adjmouse=true\n"
            "fixpitch=true\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Close Combat 4: The Battle of the Bulge\n"
            "[cc4]\n"
            "adjmouse=true\n"
            "fixpitch=true\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Close Combat 5: Invasion: Normandy\n"
            "[cc5]\n"
            "adjmouse=true\n"
            "fixpitch=true\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Commandos\n"
            "[Comandos]\n"
            "fixpitch=true\n"
            "\n"
            "; Call To Power 2\n"
            "[ctp2]\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Corsairs Gold\n"
            "[corsairs]\n"
            "adjmouse=true\n"
            "renderer=gdi\n"
            "hook=2\n"
            "\n"
            "; Dragon Throne: Battle of Red Cliffs\n"
            "[AdSanguo]\n"
            "maxgameticks=60\n"
            "noactivateapp=true\n"
            "\n"
            "; Dark Reign: The Future of War\n"
            "[DKReign]\n"
            "renderer=opengl\n"
            "maxgameticks=60\n"
            "\n"
            "; Dungeon Keeper 2\n"
            "[DKII]\n"
            "maxgameticks=60\n"
            "noactivateapp=true\n"
            "\n"
            "; Deadlock 2\n"
            "[DEADLOCK]\n"
            "fixchilds=0\n"
            "adjmouse=false\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Diablo\n"
            "[Diablo]\n"
            "devmode=true\n"
            "\n"
            "; Diablo: Hellfire\n"
            "[hellfire]\n"
            "devmode=true\n"
            "\n"
            "; Eggsucker\n"
            "[eggsucker]\n"
            "fixpitch=true\n"
            "\n"
            "; Economic War\n"
            "[EcoW]\n"
            "maxgameticks=60\n"
            "fixnotresponding=true\n"
            "\n"
            "; Future Cop - L.A.P.D.\n"
            "[FCopLAPD]\n"
            "adjmouse=true\n"
            "fixchilds=3\n"
            "\n"
            "; G-Police\n"
            "[GPOLICE]\n"
            "maxgameticks=60\n"
            "\n"
            "; Gangsters: Organized Crime\n"
            "[gangsters]\n"
            "adjmouse=true\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Grand Theft Auto\n"
            "[Grand Theft Auto]\n"
            "singlecpu=false\n"
            "\n"
            "; Grand Theft Auto: London 1969\n"
            "[gta_uk]\n"
            "singlecpu=false\n"
            "\n"
            "; Grand Theft Auto: London 1961\n"
            "[Gta_61]\n"
            "singlecpu=false\n"
            "\n"
            "; Heroes of Might and Magic II:  The Succession Wars\n"
            "[HEROES2W]\n"
            "adjmouse=true\n"
            "\n"
            "; Hard Truck: Road to Victory\n"
            "[htruck]\n"
            "maxgameticks=25\n"
            "renderer=opengl\n"
            "noactivateapp=true\n"
            "\n"
            "; Invictus\n"
            "[Invictus]\n"
            "adjmouse=true\n"
            "renderer=opengl\n"
            "\n"
            "; Interstate 76\n"
            "[i76]\n"
            "adjmouse=true\n"
            "renderer=opengl\n"
            "\n"
            "; Jagged Alliance 2\n"
            "[ja2]\n"
            "fixmousehook=true\n"
            "noactivateapp=true\n"
            "releasealt=true\n"
            "\n"
            "; Jagged Alliance 2: Wildfire\n"
            "[WF6]\n"
            "fixmousehook=true\n"
            "noactivateapp=true\n"
            "releasealt=true\n"
            "\n"
            "; Jagged Alliance 2 - UC mod\n"
            "[JA2_UC]\n"
            "fixmousehook=true\n"
            "noactivateapp=true\n"
            "releasealt=true\n"
            "\n"
            "; Kings Quest 8\n"
            "[Mask]\n"
            "renderer=opengl\n"
            "\n"
            "; Konung\n"
            "[konung]\n"
            "fixchilds=0\n"
            "\n"
            "; Konung 2\n"
            "[Konung2]\n"
            "fixchilds=0\n"
            "\n"
            "; KKND Xtreme (With high resolution patch)\n"
            "[KKNDgame]\n"
            "vhack=true\n"
            "\n"
            "; KKND2: Krossfire\n"
            "[KKND2]\n"
            "noactivateapp=true\n"
            "renderer=gdi\n"
            "hook=2\n"
            "\n"
            "; Lionheart\n"
            "[Lionheart]\n"
            "locktopleft=true\n"
            "\n"
            "; Majesty Gold\n"
            "[Majesty]\n"
            "minfps=-2\n"
            "\n"
            "; Majesty Gold HD\n"
            "[MajestyHD]\n"
            "adjmouse=true\n"
            "\n"
            "; Majesty Gold HD\n"
            "[MajestyHD - Old]\n"
            "adjmouse=true\n"
            "\n"
            "; Mech Warrior 3\n"
            "[Mech3]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Moorhuhn\n"
            "[Moorhuhn]\n"
            "renderer=opengl\n"
            "dinputhook=true\n"
            "\n"
            "; Moorhuhn 2\n"
            "[Moorhuhn2]\n"
            "dinputhook=true\n"
            "\n"
            "; Outlaws\n"
            "[olwin]\n"
            "noactivateapp=true\n"
            "maxgameticks=60\n"
            "adjmouse=true\n"
            "renderer=gdi\n"
            "\n"
            "; Pharaoh\n"
            "[Pharaoh]\n"
            "adjmouse=true\n"
            "\n"
            "; Pacific General\n"
            "[PACGEN]\n"
            "renderer=opengl\n"
            "\n"
            "; Pax Imperia\n"
            "[Pax Imperia]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Railroad Tycoon II\n"
            "[RT2]\n"
            "adjmouse=true\n"
            "\n"
            "; ROAD RASH\n"
            "[RoadRash]\n"
            "adjmouse=true\n"
            "\n"
            "; Septerra Core\n"
            "[septerra]\n"
            "hook=2\n"
            "\n"
            "; Settlers 3\n"
            "[s3]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "\n"
            "; Star Trek - Armada\n"
            "[Armada]\n"
            "armadahack=true\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "adjmouse=true\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Star Wars: Galactic Battlegrounds\n"
            "[battlegrounds]\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "\n"
            "; Star Wars: Galactic Battlegrounds: Clone Campaigns\n"
            "[battlegrounds_x1]\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "\n"
            "; Stronghold Crusader HD\n"
            "[Stronghold Crusader]\n"
            "adjmouse=true\n"
            "\n"
            "; Space Rangers\n"
            "[Rangers]\n"
            "locktopleft=true\n"
            "\n"
            "; Stronghold Crusader Extreme HD\n"
            "[Stronghold_Crusader_Extreme]\n"
            "adjmouse=true\n"
            "\n"
            "; Stronghold HD\n"
            "[Stronghold]\n"
            "adjmouse=true\n"
            "\n"
            "; Steel Panthers: World At War\n"
            "[MECH]\n"
            "renderer=opengl\n"
            "\n"
            "; Sim City 3000\n"
            "[SC3]\n"
            "renderer=opengl\n"
            "minfps=-2\n"
            "\n"
            "; Shadow Watch\n"
            "[sw]\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "\n"
            "; Shadow Flare\n"
            "[ShadowFlare]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "adjmouse=true\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Three Kingdoms: Fate of the Dragon\n"
            "[sanguo]\n"
            "maxgameticks=60\n"
            "noactivateapp=true\n"
            "\n"
            "; Twisted Metal\n"
            "[TWISTED]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "maxgameticks=25\n"
            "minfps=5\n"
            "\n"
            "; Twisted Metal 2\n"
            "[Tm2]\n"
            "renderer=opengl\n"
            "nonexclusive=true\n"
            "maxgameticks=60\n"
            "adjmouse=true\n"
            "fixchilds=1\n"
            "maintas=false\n"
            "boxing=false\n"
            "\n"
            "; Tzar: The Burden of the Crown\n"
            "; Note: Must set 'DIRECTXDEVICE=0' in 'Tzar.ini'\n"
            "[Tzar]\n"
            "adjmouse=true\n"
            "\n"
            "; Uprising\n"
            "[uprising]\n"
            "renderer=opengl\n"
            "fixpitch=true\n"
            "adjmouse=true\n"
            "\n"
            "; Uprising 2\n"
            "[Uprising 2]\n"
            "renderer=opengl\n"
            "adjmouse=true\n"
            "\n"
            "; Warlords 3\n"
            "[Darklord]\n"
            "renderer=gdi\n"
            "\n"
            "; Wizardry 8\n"
            "[Wiz8]\n"
            "fixmousehook=true\n"
            "noactivateapp=true\n"
            "releasealt=true\n"
            "\n"
            "; Worms Armageddon\n"
            "[WA]\n"
            "adjmouse=true\n"
            "width=0\n"
            "height=0\n"
            "resizable=false\n"
            "\n"
            "; Wizards and Warriors\n"
            "[deep6]\n"
            "renderer=gdi\n"
            "hook=2\n"
            "\n"
            "; War Wind\n"
            "[WW]\n"
            "renderer=opengl\n"
            "\n"
            "; Zeus and Poseidon\n"
            "[Zeus]\n"
            "adjmouse=true\n"
            "\n"

            , fh);
        fclose(fh);
    }
}

static void cfg_init()
{
    /* get process filename and directory */
    if (GetModuleFileNameA(NULL, g_config.game_path, sizeof(g_config.game_path) - 1) > 0)
    {
        _splitpath(g_config.game_path, NULL, NULL, g_config.process_file_name, NULL);

        char* end = strstr(g_config.game_path, g_config.process_file_name);

        if (end)
        {
            *end = 0;
        }
        else
        {
            g_config.game_path[0] = 0;
        }
    }

    /* set up settings ini */
    strncpy(g_config.ini_path, ".\\ddraw.ini", sizeof(g_config.ini_path) - 1);

    if (GetFileAttributes(g_config.ini_path) == INVALID_FILE_ATTRIBUTES)
        cfg_create_ini();
}

DWORD cfg_get_string(LPCSTR key, LPCSTR default_value, LPSTR out_string, DWORD out_size)
{
    if (!g_config.ini_path[0])
        cfg_init();

    DWORD s = GetPrivateProfileStringA(
        g_config.process_file_name, key, "", out_string, out_size, g_config.ini_path);

    if (s > 0)
    {
        char buf[MAX_PATH] = { 0 };

        if (GetPrivateProfileStringA(
            g_config.process_file_name, "checkfile", "", buf, sizeof(buf), g_config.ini_path) > 0)
        {
            if (GetFileAttributes(buf) != INVALID_FILE_ATTRIBUTES)
                return s;
        }
        else
            return s;
    }

    return GetPrivateProfileStringA("ddraw", key, default_value, out_string, out_size, g_config.ini_path);
}

BOOL cfg_get_bool(LPCSTR key, BOOL default_value)
{
    char value[8];
    cfg_get_string(key, default_value ? "Yes" : "No", value, sizeof(value));

    return (_stricmp(value, "yes") == 0 || _stricmp(value, "true") == 0 || _stricmp(value, "1") == 0);
}

int cfg_get_int(LPCSTR key, int default_value)
{
    char def_value[20];
    _snprintf(def_value, sizeof(def_value), "%d", default_value);

    char value[20];
    cfg_get_string(key, def_value, value, sizeof(value));

    if (strstr(value, "0x"))
    {
        return (int)strtol(value, NULL, 0);
    }
    else
    {
        return atoi(value);
    }
}
