#ifndef CONFIG_H
#define CONFIG_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


typedef struct CNCDDRAWCONFIG
{
    RECT window_rect;
    int window_state;
    int borderless_state;
    char ini_path[MAX_PATH];
    char game_path[MAX_PATH];
    char process_file_name[MAX_PATH];
    int save_settings;

} CNCDDRAWCONFIG;

extern CNCDDRAWCONFIG g_config;

void cfg_load();
void cfg_save();

BOOL cfg_get_bool(LPCSTR key, BOOL default_value);
int cfg_get_int(LPCSTR key, int default_value);
DWORD cfg_get_string(LPCSTR key, LPCSTR default_value, LPSTR out_string, DWORD out_size);

#endif
