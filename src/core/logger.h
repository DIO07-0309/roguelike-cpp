#pragma once
// Fix: windows.h defines ERROR macro
#ifdef ERROR
#undef ERROR
#endif
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

    void init(const char* dir = ".");
    void close();
    void log(LogLevel lv, const char* fmt, ...);
    void crash(const char* fmt, ...);

private:
    Logger() = default;
    ~Logger() { close(); }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

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
