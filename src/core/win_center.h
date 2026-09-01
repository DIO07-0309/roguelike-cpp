#pragma once
// 将当前活动窗口居中 (Win32 API, 避免 windows.h 污染)
void center_active_window();

// G10.7-B1: GUI 子系统下 stdio 处置 (无句柄时重定向 NUL, 有句柄保持直写)
void gui_sanitize_stdio();
