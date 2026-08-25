#ifndef UTILS_H
#define UTILS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


HMODULE WINAPI util_enumerate_modules(_In_opt_ HMODULE hModuleLast);
void util_set_process_affinity();
void util_set_thread_affinity(DWORD tid);
void util_pull_messages();
DWORD util_get_timestamp(HMODULE mod);
FARPROC util_get_iat_proc(HMODULE mod, char* module_name, char* function_name);
BOOL util_caller_is_ddraw_wrapper(void* return_address);
BOOL util_is_bad_read_ptr(void* p);
BOOL util_is_minimized(HWND hwnd);
BOOL util_in_foreground();
BOOL util_is_avx_supported();
void util_limit_game_ticks();
void util_update_bnet_pos(int newX, int newY);
BOOL util_get_lowest_resolution(float ratio, SIZE* outRes, DWORD minWidth, DWORD minHeight, DWORD maxWidth, DWORD maxHeight);
void util_toggle_maximize();
void util_toggle_fullscreen();
BOOL util_unadjust_window_rect(LPRECT prc, DWORD dwStyle, BOOL fMenu, DWORD dwExStyle);
void util_set_window_rect(int x, int y, int width, int height, UINT flags);
BOOL CALLBACK util_enum_thread_wnd_proc(HWND hwnd, LPARAM lParam);
BOOL CALLBACK util_enum_child_proc(HWND hwnd, LPARAM lParam);
BOOL util_detect_low_res_screen();
void util_get_surface_origin(int* x, int* y);

#endif
