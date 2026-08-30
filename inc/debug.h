#ifndef DEBUG_H
#define DEBUG_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

LONG WINAPI dbg_exception_handler(EXCEPTION_POINTERS* exception);
LONG WINAPI dbg_vectored_exception_handler(EXCEPTION_POINTERS* exception);
void dbg_counter_start();
double dbg_counter_stop();
void dbg_debug_string(const char* format, ...);
void dbg_print_rect(char* info, LPRECT rect);
void dbg_draw_frame_info_start();
void dbg_draw_frame_info_end();
void dbg_printf(const char* fmt, ...);
void dbg_init();
void dbg_dump_wnd_styles(DWORD style, DWORD exstyle);
void dbg_dump_swp_flags(DWORD flags);
void dbg_dump_ddp_flags(DWORD flags);
void dbg_dump_scl_flags(DWORD flags);
void dbg_dump_edm_flags(DWORD flags);
void dbg_dump_dds_flip_flags(DWORD flags);
void dbg_dump_dds_blt_flags(DWORD flags);
void dbg_dump_dds_blt_fx_flags(DWORD flags);
void dbg_dump_dds_caps(DWORD caps);
void dbg_dump_dds_flags(DWORD flags);
void dbg_dump_dds_blt_fast_flags(DWORD flags);
void dbg_dump_dds_lock_flags(DWORD flags);
void dbg_dump_di_scm_flags(DWORD flags);
void dbg_dump_hook_type(int idHook);
char* dbg_d3d9_hr_to_str(HRESULT hr);
char* dbg_mes_to_str(int id);
void __cdecl dbg_invoke_watson(wchar_t const*, wchar_t const*, wchar_t const*, unsigned int, uintptr_t);

extern double g_dbg_frame_time;
extern DWORD g_dbg_frame_count;
extern LPTOP_LEVEL_EXCEPTION_FILTER g_dbg_exception_filter;
extern PVOID g_dbg_exception_handle;

#if defined(__GNUC__) /* wrap msvc intrinsics onto gcc builtins */
#undef  _ReturnAddress
#undef  _AddressOfReturnAddress
#define _ReturnAddress()		__builtin_return_address(0)
#define _AddressOfReturnAddress()	__builtin_frame_address (0)
#else
#pragma intrinsic(_ReturnAddress)
#endif /* __GNUC__ */

//#define _DEBUG 1

/* use OutputDebugStringA rather than printf */
//#define _DEBUG_S 1

/* log everything (slow) */
//#define _DEBUG_X 1

/* disable noisy detail groups on demand (default: enabled when _DEBUG_X is enabled) */
//#define _DEBUG_NO_LOCK 1
//#define _DEBUG_NO_CHILD_ENUM 1
//#define _DEBUG_NO_BLT 1

#ifdef _DEBUG_X
#ifndef _DEBUG_NO_LOCK
#define _DEBUG_LOCK 1
#endif
#ifndef _DEBUG_NO_CHILD_ENUM
#define _DEBUG_CHILD_ENUM 1
#endif
#ifndef _DEBUG_NO_BLT
#define _DEBUG_BLT 1
#endif
#endif



#ifdef _DEBUG

#ifdef _DEBUG_S

#define TRACE(format, ...) dbg_debug_string("xDBG " format, ##__VA_ARGS__)

#ifdef _DEBUG_X
#define TRACE_EXT(format, ...) dbg_debug_string("xDBG " format, ##__VA_ARGS__)
#else
#define TRACE_EXT(format, ...)
#endif

#if defined(_DEBUG_X) && defined(_DEBUG_LOCK)
#define TRACE_LOCK(format, ...) dbg_debug_string("xDBG " format, ##__VA_ARGS__)
#else
#define TRACE_LOCK(format, ...)
#endif

#if defined(_DEBUG_X) && defined(_DEBUG_BLT)
#define TRACE_BLT(format, ...) dbg_debug_string("xDBG " format, ##__VA_ARGS__)
#else
#define TRACE_BLT(format, ...)
#endif

#else

#define TRACE(format, ...) dbg_printf(format, ##__VA_ARGS__) 

#ifdef _DEBUG_X
#define TRACE_EXT(format, ...) dbg_printf(format, ##__VA_ARGS__) 
#else
#define TRACE_EXT(format, ...)
#endif

#if defined(_DEBUG_X) && defined(_DEBUG_LOCK)
#define TRACE_LOCK(format, ...) dbg_printf(format, ##__VA_ARGS__)
#else
#define TRACE_LOCK(format, ...)
#endif

#if defined(_DEBUG_X) && defined(_DEBUG_BLT)
#define TRACE_BLT(format, ...) dbg_printf(format, ##__VA_ARGS__)
#else
#define TRACE_BLT(format, ...)
#endif

#endif 

#else 
#define TRACE(format, ...)
#define TRACE_EXT(format, ...)
#define TRACE_LOCK(format, ...)
#define TRACE_BLT(format, ...)
#endif

#endif
