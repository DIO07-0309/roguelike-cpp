// 独立编译单元，用 Win32 API 居中窗口，不与 raylib.h 冲突
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <fcntl.h>
#include <io.h>

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

// G10.7-B1: WIN32 GUI 子系统 stdio 接线 — MinGW GUI CRT 默认丢弃 stdout,
// 即使进程被外部重定向(管道/文件)。此函数在有句柄时用 dup2 把标准 fd
// 重新指向 OS 句柄, 恢复 sim 平衡报告/CI 输出; 双击运行(无句柄) NUL 兜底。
void gui_sanitize_stdio();
void gui_sanitize_stdio() {
    auto attach = [](DWORD which, int stdfd) -> bool {
        HANDLE h = GetStdHandle(which);
        if (h == NULL || h == INVALID_HANDLE_VALUE) return false;
        int osfd = _open_osfhandle((intptr_t)h, _O_WRONLY);
        if (osfd < 0) return false;
        if (_dup2(osfd, stdfd) < 0) { _close(osfd); return false; }
        // dup2 后 fd 归 stdfd 所有; osfd 若与 stdfd 不同则关闭副本句柄描述符
        // (OS 句柄本身保持两处引用, 关闭一个不影响另一个)
        return true;
    };
    bool out_ok = attach(STD_OUTPUT_HANDLE, fileno(stdout));
    bool err_ok = attach(STD_ERROR_HANDLE, fileno(stderr));
    if (!out_ok) freopen("NUL:", "w", stdout);
    if (!err_ok) freopen("NUL:", "w", stderr);
}
