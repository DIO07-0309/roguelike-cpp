// ============================================================
// Windows SEH 崩溃捕获 — 独立编译单元，零外部依赖
// ============================================================
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ctime>
#include <cstdio>

// 独立崩溃日志 (不依赖 Logger 单例，崩溃时它可能已死)
static void _write_crash(const char* msg) {
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    FILE* cf = fopen("crash.log", "a");
    if (cf) {
        fprintf(cf, "[%s] CRASH\n%s\n", ts, msg);
        fclose(cf);
    }
}

extern "C" long __stdcall _seh_handler(void* pinfo) {
    auto* info = (EXCEPTION_POINTERS*)pinfo;
    DWORD code = info->ExceptionRecord->ExceptionCode;
    void* addr = info->ExceptionRecord->ExceptionAddress;

    char buf[256];
    snprintf(buf, sizeof(buf),
        "SEH Exception 0x%08X at 0x%p\n"
        "开发者：周禹同 20252450",
        (unsigned)code, addr);
    _write_crash(buf);

    return (long)EXCEPTION_EXECUTE_HANDLER;
}

void install_seh_handler() {
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)_seh_handler);
}
#endif
