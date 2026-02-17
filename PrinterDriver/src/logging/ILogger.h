#pragma once
#include <cstdarg>

enum LogCategory {
    LOG_CATEGORY_NONE = 0,
    LOG_CATEGORY_ERROR = 1 << 0,
    LOG_CATEGORY_WARNING = 1 << 1,
    LOG_CATEGORY_INFO = 1 << 2,
    LOG_CATEGORY_DEBUG = 1 << 3,
    LOG_CATEGORY_MOTOR = 1 << 4,
    LOG_CATEGORY_ENCODER = 1 << 5,
    LOG_CATEGORY_BLUETOOTH = 1 << 6,
    LOG_CATEGORY_PROFILE = 1 << 7,
    LOG_CATEGORY_PERFORMANCE = 1 << 8,
    LOG_CATEGORY_COMMAND = 1 << 9, 

    LOG_CATEGORY_ALL = 0xFFFFFFFF,
    LOG_CATEGORY_DEFAULT = LOG_CATEGORY_ERROR | LOG_CATEGORY_WARNING |
    LOG_CATEGORY_INFO | LOG_CATEGORY_MOTOR |
    LOG_CATEGORY_ENCODER,

#ifdef _DEBUG
    LOG_CATEGORY_RELEASE = LOG_CATEGORY_ALL,
#else
    LOG_CATEGORY_RELEASE = LOG_CATEGORY_ERROR | LOG_CATEGORY_WARNING | LOG_CATEGORY_INFO,
#endif
};

class ILogger {
public:
    ILogger() = default;
    virtual ~ILogger() = default;

    ILogger(const ILogger&) = delete;
    ILogger& operator=(const ILogger&) = delete;
    ILogger(ILogger&&) = delete;
    ILogger& operator=(ILogger&&) = delete;

    virtual void log(LogCategory category, const char* format, ...) = 0;

    virtual void logV(LogCategory category, const char* format, va_list args) = 0;

    virtual bool isEnabled(LogCategory category) const = 0;

#if defined(_WIN32)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

    FORCE_INLINE void error(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_ERROR)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_ERROR, format, args);
        va_end(args);
    }

    FORCE_INLINE void warning(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_WARNING)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_WARNING, format, args);
        va_end(args);
    }

    FORCE_INLINE void info(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_INFO)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_INFO, format, args);
        va_end(args);
    }

    FORCE_INLINE void debug(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_DEBUG)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_DEBUG, format, args);
        va_end(args);
    }

    FORCE_INLINE void motor(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_MOTOR)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_MOTOR, format, args);
        va_end(args);
    }

    FORCE_INLINE void encoder(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_ENCODER)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_ENCODER, format, args);
        va_end(args);
    }

    FORCE_INLINE void bluetooth(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_BLUETOOTH)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_BLUETOOTH, format, args);
        va_end(args);
    }

    FORCE_INLINE void profile(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_PROFILE)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_PROFILE, format, args);
        va_end(args);
    }

    FORCE_INLINE void performance(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_PERFORMANCE)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_PERFORMANCE, format, args);
        va_end(args);
    }

    FORCE_INLINE void command(const char* format, ...) {
        if (!isEnabled(LOG_CATEGORY_COMMAND)) return;
        va_list args;
        va_start(args, format);
        logV(LOG_CATEGORY_COMMAND, format, args);
        va_end(args);
    }
};