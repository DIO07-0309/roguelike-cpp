// ============================================================
// Windows SEH 崩溃捕获 — 独立编译单元，零外部依赖
// P0-M1 诊断增强: CaptureStackBackTrace + dbghelp 符号化 (系统自带)
// ============================================================
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ctime>
#include <cstdio>
#include <cstdint>
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

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

static void _write_stack(EXCEPTION_POINTERS* info) {
    FILE* cf = fopen("crash.log", "a");
    if (!cf) return;
    void* frames[32];
    USHORT n = CaptureStackBackTrace(0, 32, frames, nullptr);
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    fprintf(cf, "---- stack trace (%u frames) ----\n", (unsigned)n);
    SYMBOL_INFOW* sym = (SYMBOL_INFOW*)calloc(sizeof(SYMBOL_INFOW) + 256 * sizeof(wchar_t), 1);
    for (USHORT i = 0; i < n; i++) {
        DWORD64 disp = 0;
        sym->SizeOfStruct = sizeof(SYMBOL_INFOW);
        sym->MaxNameLen = 255;
        DWORD64 addr = (DWORD64)(uintptr_t)frames[i];
        IMAGEHLP_MODULEW64 mod = {};
        mod.SizeOfStruct = sizeof(mod);
        wchar_t modname[64] = L"?";
        if (SymGetModuleInfoW64(GetCurrentProcess(), addr, &mod))
            wcscpy_s(modname, 64, mod.ModuleName);
        if (SymFromAddrW(GetCurrentProcess(), addr, &disp, sym)) {
            fwprintf(cf, L"  #%02u %s!%s +0x%llx\n",
                     (unsigned)i, modname, sym->Name, (unsigned long long)disp);
        } else {
            fwprintf(cf, L"  #%02u %s!0x%llx (no symbol)\n",
                     (unsigned)i, modname, (unsigned long long)addr);
        }
    }
    free(sym);
    fprintf(cf, "---- end stack ----\n");
    fclose(cf);
}

extern "C" long __stdcall _seh_handler(void* pinfo) {
    auto* info = (EXCEPTION_POINTERS*)pinfo;
    DWORD code = info->ExceptionRecord->ExceptionCode;
    void* addr = info->ExceptionRecord->ExceptionAddress;

    void* base = (void*)GetModuleHandleA(NULL);  // 主模块基址
    uint64_t rva = base ? (uintptr_t)addr - (uintptr_t)base : 0;

    char buf[256];
    snprintf(buf, sizeof(buf),
        "SEH Exception 0x%08X at 0x%p (RVA 0x%llX, base 0x%p)\n"
        "开发者：ruozhiDIO",
        (unsigned)code, addr, (unsigned long long)rva, base);
    _write_crash(buf);
    _write_stack(info);

    return (long)EXCEPTION_EXECUTE_HANDLER;
}

void install_seh_handler() {
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)_seh_handler);
}
#endif
