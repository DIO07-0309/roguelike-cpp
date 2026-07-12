#include "logger.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>

void Logger::init(const char* dir) {
    _dir = dir;
    std::string path = _dir + "/game.log";
    _fp = fopen(path.c_str(), "a");
    if (_fp) {
        fprintf(_fp, "\n======== ROGUELIKE C++ EDITION ========\n");
        fflush(_fp);
    }
}

void Logger::close() {
    if (_fp) { fclose(_fp); _fp = nullptr; }
}

void Logger::log(LogLevel lv, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(_mtx);
    const char* labels[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // timestamp
    time_t now = time(nullptr);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    if (_fp) {
        fprintf(_fp, "[%s] [%s] %s\n", ts, labels[(int)lv], buf);
        fflush(_fp);
    }
    printf("[%s] %s\n", labels[(int)lv], buf);
}

void Logger::crash(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    time_t now = time(nullptr);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    // crash.log
    std::string path = _dir + "/crash.log";
    FILE* cf = fopen(path.c_str(), "w");
    if (cf) {
        fprintf(cf, "[%s] CRASH\n%s\n", ts, buf);
        fclose(cf);
    }
    if (_fp) {
        fprintf(_fp, "[%s] [FATAL] CRASH: %s\n", ts, buf);
        fflush(_fp);
        fclose(_fp);
        _fp = nullptr;
    }
}
