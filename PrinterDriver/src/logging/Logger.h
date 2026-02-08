#pragma once

#ifdef _DEBUG
#define LOG_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#else
#define LOG_ENABLED 1
#define LOG_DEBUG_ENABLED 0
#endif

// Basic logging macros with category checking
#define LOG_ERROR(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_ERROR)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_ERROR, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_WARNING(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_WARNING)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_WARNING, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_INFO(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_INFO)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_INFO, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_DEBUG(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_DEBUG)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_DEBUG, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_MOTOR(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_MOTOR)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_MOTOR, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_ENCODER(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_ENCODER)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_ENCODER, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_BLUETOOTH(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_BLUETOOTH)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_BLUETOOTH, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_PROFILE(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_PROFILE)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_PROFILE, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_PERFORMANCE(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_PERFORMANCE)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_PERFORMANCE, format, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_COMMAND(format, ...) \
    do { \
        if (Logger::instance().isCategoryEnabled(LOG_CATEGORY_COMMAND)) { \
            Logger::instance().addLogInternal(LOG_CATEGORY_COMMAND, format, ##__VA_ARGS__); \
        } \
    } while(0)

#if defined(_WIN32)
    #define FORCE_INLINE __forceinline
    #define LOCALTIME(tm, time) localtime_s(tm, time)
    #define STRCPY_SAFE(dest, src, size) strcpy_s(dest, size, src)
    #define STRNCAT_SAFE(dest, src, size) strncat_s(dest, size, src, _TRUNCATE)
    #define STRNCMP_SAFE(s1, s2, size) strncmp_s(s1, s2, size)
    #define STRNCASECMP_SAFE(s1, s2, size) _strnicmp(s1, s2, size)
    #define STRNCPY_SAFE(dest, src, destSize, count) strncpy_s(dest, destSize, src, count)
#else
    #define FORCE_INLINE inline __attribute__((always_inline))
    #define LOCALTIME(tm, time) localtime_r(time, tm)
    #define STRCPY_SAFE(dest, src, size) strncpy(dest, src, size)
    #define STRNCAT_SAFE(dest, src, size) strncat(dest, src, size)
    #define STRNCMP_SAFE(s1, s2, size) strncmp(s1, s2, size)
    #define STRNCASECMP_SAFE(s1, s2, size) strncasecmp(s1, s2, size)
    #define STRNCPY_SAFE(dest, src, destSize, count) do { \
        size_t n = (count) < (destSize) ? (count) : (destSize)-1; \
        strncpy(dest, src, n); \
        dest[n] = '\0'; \
    } while(0)
#endif

#include <cstdint>
#include <cstdarg>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>

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

class Logger {
public:
    // Configuration
    static constexpr size_t MAX_LOG_ENTRIES = 10000;
    static constexpr size_t MAX_MESSAGE_LENGTH = 1023;

    // Singleton access
    static Logger& instance();

    void addLogInternal(LogCategory category, const char* format, ...);

    // Manage categories
    void setEnabledCategories(uint32_t categories);
    uint32_t getEnabledCategories() const;

    FORCE_INLINE static bool isCategoryEnabled(LogCategory category) {
        return (enabledCategories.load(std::memory_order_relaxed) & category) != 0;
    }

    template<size_t N>
    FORCE_INLINE void formatToBuffer(char(&buffer)[N], const char* format, va_list args) {
        vsnprintf(buffer, N, format, args);
    }

    // Working with the log buffer
    int getLogCount() const;
    const char* getLogEntry(int index, LogCategory* outCategory = nullptr);
    void clearLog();

    // Formatting (helper methods)
    static std::string formatTime(const std::chrono::system_clock::time_point& time);
    static const char* categoryToString(LogCategory category);

private:
    Logger();
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    struct LogEntry {
        char message[MAX_MESSAGE_LENGTH + 1];
        LogCategory category;
        std::chrono::system_clock::time_point timestamp;
    };

    // Ring buffer
    std::unique_ptr<LogEntry[]> logBuffer;
    std::atomic<size_t> logWriteIndex{ 0 };
    std::atomic<size_t> logReadIndex{ 0 };
    std::mutex logBufferMutex;

    static std::atomic<uint32_t> enabledCategories;    
};