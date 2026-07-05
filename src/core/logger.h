#pragma once
#include <string>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <mutex>

// ============================================================
// Logger — 单例，同时写入 game.log 和 crash.log
// 崩溃时写 crash.log，运行中写 game.log
// ============================================================

enum class LogLevel { DEBUG, INFO, WARN, ERROR, FATAL };

class Logger {
public:
    static Logger& inst() {
        static Logger s;
        return s;
    }

    void init(const char* dir = ".") {
        _dir = dir;
        std::string path = _dir + "/game.log";
        _fp = fopen(path.c_str(), "a");
        if (_fp) {
            fprintf(_fp, "\n======== ROGUELIKE C++ EDITION ========\n");
            fflush(_fp);
        }
    }

    void close() {
        if (_fp) { fclose(_fp); _fp = nullptr; }
    }

    void log(LogLevel lv, const char* fmt, ...) {
        std::lock_guard<std::mutex> lock(_mtx);
        const char* labels[] = {"DEBUG","INFO","WARN","ERROR","FATAL"};
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // 时间戳
        time_t now = time(nullptr);
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

        // 写入文件
        if (_fp) {
            fprintf(_fp, "[%s] [%s] %s\n", ts, labels[(int)lv], buf);
            fflush(_fp);
        }
        // 同时输出到控制台（调试用）
        printf("[%s] %s\n", labels[(int)lv], buf);
    }

    void crash(const char* fmt, ...) {
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

private:
    Logger() = default;
    ~Logger() { close(); }
    FILE* _fp = nullptr;
    std::string _dir = ".";
    std::mutex _mtx;
};

// 便捷宏
#define LOG_DEBUG(fmt, ...) Logger::inst().log(LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Logger::inst().log(LogLevel::INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::inst().log(LogLevel::WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::inst().log(LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) Logger::inst().log(LogLevel::FATAL, fmt, ##__VA_ARGS__)
#define LOG_CRASH(fmt, ...) Logger::inst().crash(fmt, ##__VA_ARGS__)

// Windows SEH 崩溃捕获 (定义在 seh_handler.cpp 独立编译单元)
#ifdef _WIN32
void install_seh_handler();
#endif
