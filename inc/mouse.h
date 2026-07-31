#ifndef MOUSE_H
#define MOUSE_H

void mouse_lock();
void mouse_unlock();
void mouse_update_clip();
LRESULT CALLBACK mouse_hook_proc(int Code, WPARAM wParam, LPARAM lParam);

extern BOOL g_mouse_locked;
extern HHOOK g_mouse_hook;
extern HOOKPROC g_mouse_proc;

#endif
