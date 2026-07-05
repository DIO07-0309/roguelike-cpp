// 独立编译单元，用 Win32 API 居中窗口，不与 raylib.h 冲突
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void center_active_window() {
    HWND hwnd = GetActiveWindow();
    if (!hwnd) return;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int ww = rc.right - rc.left;
    int wh = rc.bottom - rc.top;

    SetWindowPos(hwnd, nullptr,
                 (sw - ww) / 2, (sh - wh) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}
