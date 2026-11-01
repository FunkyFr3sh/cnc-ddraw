/* Exercise production input hooks; LTO removes unrelated renderer dependencies. */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include "dd.h"
#include "config.h"
#include "hook.h"
#include "keyboard.h"
#include "mouse.h"
#include "winapi_hooks.h"
#include "ddsurface.h"
#include "utils.h"
#include "wndproc.h"

void HandleMessage(LPMSG, HWND, UINT, UINT, UINT);

CNCDDRAW g_ddraw;
CNCDDRAWCONFIG g_config;
BOOL g_mouse_locked = TRUE;
MAPWINDOWPOINTSPROC real_MapWindowPoints = MapWindowPoints;
SCREENTOCLIENTPROC real_ScreenToClient = ScreenToClient;
GETCLIENTRECTPROC real_GetClientRect = GetClientRect;
GETWINDOWRECTPROC real_GetWindowRect = GetWindowRect;
GETWINDOWLONGAPROC real_GetWindowLongA = GetWindowLongA;
SETWINDOWLONGAPROC real_SetWindowLongA = SetWindowLongA;
SETWINDOWPOSPROC real_SetWindowPos = SetWindowPos;
GETFOREGROUNDWINDOWPROC real_GetForegroundWindow = GetForegroundWindow;
CREATEWINDOWEXAPROC real_CreateWindowExA = CreateWindowExA;
CLIENTTOSCREENPROC real_ClientToScreen = ClientToScreen;
BITBLTPROC real_BitBlt = BitBlt;
GETCURSORPOSPROC real_GetCursorPos = GetCursorPos;
SETCURSORPOSPROC real_SetCursorPos = SetCursorPos;
GETASYNCKEYSTATEPROC real_GetAsyncKeyState = GetAsyncKeyState;
static DWORD queued_pos;
static DWORD WINAPI queued_position(void) { return queued_pos; }
GETMESSAGEPOSPROC real_GetMessagePos = queued_position;
static unsigned toggles, maximizes, screenshots, unlocks, failures, checks;
void mouse_lock(void) { g_mouse_locked = TRUE; }
void mouse_unlock(void) { ++unlocks; }
void util_toggle_fullscreen(void) { ++toggles; }
void util_toggle_maximize(void) { ++maximizes; }
BOOL ss_take_screenshot(IDirectDrawSurfaceImpl *unused) { ++screenshots; return TRUE; }
HRESULT dd_SetDisplayMode(DWORD w, DWORD h, DWORD bpp, DWORD flags) { return DD_OK; }
void util_set_window_rect(int x, int y, int w, int h, UINT flags) {}
void dbg_dump_wnd_styles(DWORD style, DWORD exstyle) {}
HRESULT dds_GetDC(IDirectDrawSurfaceImpl *surface, HDC *dc) { *dc = NULL; return DDERR_GENERIC; }
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #expr); } } while (0)

static MSG mouse_message(HWND hwnd, UINT message, int x, int y)
{
    MSG msg = {0};
    msg.hwnd = hwnd;
    msg.message = message;
    msg.lParam = MAKELPARAM(x, y);
    msg.pt.x = x; msg.pt.y = y;
    MapWindowPoints(hwnd, HWND_DESKTOP, &msg.pt, 1);
    return msg;
}
static void handle(MSG *msg, HWND filter, UINT remove)
{
    HandleMessage(msg, filter, 0, 0, remove);
}

static LRESULT CALLBACK test_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_HIDE_CHILD) {
        util_hide_child_window((HWND)wp);
        return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}
static DWORD WINAPI enumerate_from_renderer(void *child)
{
    EnterCriticalSection(&g_ddraw.cs);
    util_enum_child_proc((HWND)child,0);
    LeaveCriticalSection(&g_ddraw.cs);
    return 0;
}

int main(void)
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = test_wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "InputRegression";
    RegisterClassA(&wc);
    HWND root = CreateWindowA(wc.lpszClassName,"cnc-ddraw input regression",WS_POPUP|WS_VISIBLE,50,50,960,720,NULL,NULL,wc.hInstance,NULL);
    HWND panel = CreateWindowA(wc.lpszClassName,"panel",WS_CHILD|WS_VISIBLE,50,70,590,410,root,NULL,wc.hInstance,NULL);
    HWND button = CreateWindowA(wc.lpszClassName,"button",WS_CHILD|WS_VISIBLE,400,300,100,50,panel,NULL,wc.hInstance,NULL);
    HWND other = CreateWindowA(wc.lpszClassName,"unrelated",WS_POPUP,0,0,100,100,NULL,NULL,wc.hInstance,NULL);
    CHECK(root && panel && button && other);
    g_ddraw.hwnd = root; g_ddraw.ref = 1;
    g_ddraw.width = 640; g_ddraw.height = 480;
    g_ddraw.mouse.unscale_x = g_ddraw.mouse.unscale_y = 2.0f/3.0f;
    g_config.fixchilds = 5; g_config.adjmouse = TRUE; g_config.windowed = TRUE;
    MSG msg = mouse_message(root,WM_LBUTTONDOWN,720,600);
    handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd == button);
    CHECK(GET_X_LPARAM(msg.lParam)==30 && GET_Y_LPARAM(msg.lParam)==30);
    CHECK(msg.pt.x==480 && msg.pt.y==400);
    CHECK(g_ddraw.cursor.x==480 && g_ddraw.cursor.y==400);

    /* The OS may originally target a different child at unscaled coordinates. */
    msg = mouse_message(panel,WM_MOUSEMOVE,670,530);
    handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==button && GET_X_LPARAM(msg.lParam)==30 && GET_Y_LPARAM(msg.lParam)==30);

    /* NOREMOVE returns the same transformed message without changing the queue. */
    for (int i=0; i<3; ++i) {
        msg=mouse_message(root,WM_LBUTTONDOWN,720,600);
        handle(&msg,NULL,PM_NOREMOVE);
        CHECK(msg.hwnd==button && GET_X_LPARAM(msg.lParam)==30);
    }
    /* Transparent windows are still valid input targets in hide mode. */
    SetWindowLongA(panel,GWL_EXSTYLE,WS_EX_TRANSPARENT);
    msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==button);
    EnableWindow(button,FALSE);
    msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==panel);
    EnableWindow(button,TRUE);
    ShowWindow(button,SW_HIDE);
    msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==panel);
    ShowWindow(button,SW_SHOW);

    /* Captured drag coordinates must remain signed, including outside the root. */
    SetCapture(button);
    msg=mouse_message(root,WM_MOUSEMOVE,-30,-45); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==button && GET_X_LPARAM(msg.lParam)==-470 && GET_Y_LPARAM(msg.lParam)==-400);
    ReleaseCapture();
    SetCapture(other);
    msg=mouse_message(root,WM_MOUSEMOVE,720,600); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==root);
    ReleaseCapture();

    msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,other,PM_REMOVE);
    CHECK(msg.hwnd==root);
    msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,panel,PM_REMOVE);
    CHECK(msg.hwnd==button);
    msg=mouse_message(other,WM_LBUTTONDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==other && GET_X_LPARAM(msg.lParam)==720);
    msg=mouse_message(root,WM_KEYDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==root && GET_X_LPARAM(msg.lParam)==720);

    /* Letterboxing and disabled scaling. */
    g_ddraw.mouse.x_adjust=10; g_ddraw.mouse.y_adjust=20;
    msg=mouse_message(root,WM_LBUTTONDOWN,730,620); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==button && GET_X_LPARAM(msg.lParam)==30 && GET_Y_LPARAM(msg.lParam)==30);
    g_ddraw.mouse.x_adjust=g_ddraw.mouse.y_adjust=0;
    g_config.adjmouse=FALSE;
    msg=mouse_message(root,WM_LBUTTONDOWN,480,400); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==button && GET_X_LPARAM(msg.lParam)==30);
    g_config.adjmouse=TRUE;
    g_mouse_locked=FALSE;
    msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
    CHECK(msg.hwnd==root);
    g_mouse_locked=TRUE;

    /* Modes 0..4 must not acquire the new child retargeting behavior. */
    for (int mode=0;mode<5;++mode) {
        g_config.fixchilds=mode;
        msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
        CHECK(msg.hwnd==root && GET_X_LPARAM(msg.lParam)==720);
    }

    /* Legacy root scaling is unchanged when hook_peekmessage is enabled. */
    g_config.hook_peekmessage=TRUE;
    for (int mode=0;mode<5;++mode) {
        g_config.fixchilds=mode;
        msg=mouse_message(root,WM_LBUTTONDOWN,720,600); handle(&msg,NULL,PM_REMOVE);
        CHECK(msg.hwnd==root && GET_X_LPARAM(msg.lParam)==480 && GET_Y_LPARAM(msg.lParam)==400);
    }
    g_config.hook_peekmessage=FALSE;

    POINT queued={720,600}; ClientToScreen(root,&queued);
    queued_pos=MAKELONG(queued.x,queued.y);
    g_config.fixchilds=5;
    CHECK(fake_GetMessagePos()==MAKELONG(480,400));
    queued.x=-30; queued.y=-45; ClientToScreen(root,&queued);
    queued_pos=MAKELONG(queued.x,queued.y);
    CHECK(fake_GetMessagePos()==MAKELONG(-20,-30));
    for (int mode=0;mode<5;++mode) {
        g_config.fixchilds=mode;
        CHECK(fake_GetMessagePos()==queued_pos);
    }
    g_config.fixchilds=5; g_ddraw.ref=0;
    CHECK(fake_GetMessagePos()==queued_pos);
    g_ddraw.ref=1;

    g_config.hotkeys.toggle_fullscreen=VK_RETURN;
    g_config.hotkeys.toggle_fullscreen2=VK_F11;
    g_config.hotkeys.toggle_maximize=VK_NEXT;
    g_config.hotkeys.toggle_maximize2=VK_F10;
    g_config.hotkeys.screenshot=VK_SNAPSHOT;
    LPARAM alt=(LPARAM)(1u<<29), repeat=(LPARAM)(1u<<30), up=(LPARAM)(1u<<31);
    for (int i=0;i<4;++i) keyboard_hook_proc(HC_NOREMOVE,VK_RETURN,alt);
    CHECK(toggles==0);
    keyboard_hook_proc(HC_ACTION,VK_RETURN,alt);
    CHECK(toggles==1);
    keyboard_hook_proc(HC_ACTION,VK_RETURN,alt|repeat);
    keyboard_hook_proc(HC_ACTION,VK_RETURN,alt|repeat|up);
    CHECK(toggles==1);
    keyboard_hook_proc(HC_NOREMOVE,VK_F11,0);
    CHECK(toggles==1);
    keyboard_hook_proc(HC_ACTION,VK_F11,0);
    CHECK(toggles==2);
    keyboard_hook_proc(HC_NOREMOVE,VK_NEXT,alt);
    keyboard_hook_proc(HC_NOREMOVE,VK_F10,0);
    CHECK(maximizes==0);
    keyboard_hook_proc(HC_ACTION,VK_NEXT,alt);
    keyboard_hook_proc(HC_ACTION,VK_F10,0);
    CHECK(maximizes==2);
    keyboard_hook_proc(HC_NOREMOVE,VK_SNAPSHOT,up);
    CHECK(screenshots==0);
    keyboard_hook_proc(HC_ACTION,VK_SNAPSHOT,up);
    CHECK(screenshots==1);
    g_ddraw.alt_key_down=FALSE;
    keyboard_hook_proc(HC_NOREMOVE,VK_MENU,alt);
    CHECK(!g_ddraw.alt_key_down);
    keyboard_hook_proc(HC_ACTION,VK_MENU,alt);
    CHECK(g_ddraw.alt_key_down);
    keyboard_hook_proc(-1,VK_RETURN,alt);
    keyboard_hook_proc(HC_ACTION,'A',0);
    CHECK(toggles==2 && maximizes==2 && screenshots==1 && unlocks==0);
    /* A topmost logical overlay must precede older child screens, without
     * changing ordinary children or the existing compatibility modes. */
    SetForegroundWindow(root); SetFocus(panel);
    for (int mode=0;mode<6;++mode) {
        g_config.fixchilds=mode;
        HWND overlay=fake_CreateWindowExA(WS_EX_TOPMOST,wc.lpszClassName,"overlay",
            WS_CHILD|WS_VISIBLE,0,0,640,480,root,NULL,wc.hInstance,NULL);
        CHECK(overlay != NULL);
        CHECK((GetWindow(root,GW_CHILD)==overlay)==(mode==5));
        CHECK(GetFocus()==(mode==5 ? overlay : panel));
        DestroyWindow(overlay); SetFocus(panel);
    }
    g_config.fixchilds=5;
    HWND ordinary=fake_CreateWindowExA(0,wc.lpszClassName,"ordinary",
        WS_CHILD|WS_VISIBLE,0,0,640,480,root,NULL,wc.hInstance,NULL);
    CHECK(GetWindow(root,GW_CHILD)!=ordinary && GetFocus()==panel);
    HWND passive=fake_CreateWindowExA(WS_EX_TOPMOST|WS_EX_NOACTIVATE,wc.lpszClassName,"passive",
        WS_CHILD|WS_VISIBLE,0,0,640,480,root,NULL,wc.hInstance,NULL);
    CHECK(GetWindow(root,GW_CHILD)==passive && GetFocus()==panel);
    DestroyWindow(passive);
    HWND hidden=fake_CreateWindowExA(WS_EX_TOPMOST,wc.lpszClassName,"hidden",
        WS_CHILD,0,0,640,480,root,NULL,wc.hInstance,NULL);
    CHECK(GetFocus()==panel);
    DestroyWindow(hidden);
    SetWindowLongA(panel,GWL_EXSTYLE,0);
    g_config.fixchilds=3;
    CHECK(!util_enum_child_proc(panel,0));
    CHECK(GetWindowLongA(panel,GWL_EXSTYLE)&WS_EX_TRANSPARENT);
    CHECK(!(GetWindowLongA(ordinary,GWL_EXSTYLE)&WS_EX_TRANSPARENT));
    g_config.fixchilds=5;
    CHECK(EnumChildWindows(root,util_enum_child_proc,0));
    CHECK(GetWindowLongA(ordinary,GWL_EXSTYLE)&WS_EX_TRANSPARENT);

    /* The renderer must release its drawing lock even while the UI thread is
     * not pumping messages. SetWindowLong from that thread deadlocks here. */
    InitializeCriticalSection(&g_ddraw.cs);
    for (int mode=3;mode<=5;++mode) {
        g_config.fixchilds=mode;
        SetWindowLongA(ordinary,GWL_EXSTYLE,WS_EX_TOOLWINDOW);
        SetFocus(panel);
        HANDLE worker=CreateThread(NULL,0,enumerate_from_renderer,ordinary,0,NULL);
        CHECK(worker != NULL);
        CHECK(WaitForSingleObject(worker,500)==WAIT_OBJECT_0);
        MSG pending;
        while (PeekMessageA(&pending,NULL,0,0,PM_REMOVE)) DispatchMessageA(&pending);
        for (int tries=0;tries<20 && WaitForSingleObject(worker,0)!=WAIT_OBJECT_0;++tries) {
            MsgWaitForMultipleObjects(1,&worker,FALSE,100,QS_ALLINPUT);
            while (PeekMessageA(&pending,NULL,0,0,PM_REMOVE)) DispatchMessageA(&pending);
        }
        CHECK(WaitForSingleObject(worker,0)==WAIT_OBJECT_0);
        if (WaitForSingleObject(worker,0)!=WAIT_OBJECT_0) return 2;
        CHECK((GetWindowLongA(ordinary,GWL_EXSTYLE)&(WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW))
              ==(WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW));
        CHECK(GetFocus()==panel);
        CloseHandle(worker);
    }
    DeleteCriticalSection(&g_ddraw.cs);
    /* A queued request must not affect a window reparented before dispatch. */
    SetWindowLongA(ordinary,GWL_EXSTYLE,0);
    SetParent(ordinary,other);
    util_hide_child_window(ordinary);
    CHECK(!(GetWindowLongA(ordinary,GWL_EXSTYLE)&WS_EX_TRANSPARENT));
    DestroyWindow(ordinary);
    util_hide_child_window(ordinary);
    DestroyWindow(root); DestroyWindow(other);
    printf("%u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
