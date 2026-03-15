#pragma once

#include <cstdarg>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <memory>
#include <cstring>
#include <ctime>

#if defined(_MSC_VER)
#include <intrin.h>
    #define CTTZ(x) _tzcnt_u32(x)
#else
    #define CTTZ(x) __builtin_ctz(x)
#endif

// Logging categories (bit mask)
enum LogCategory : uint32_t {
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
    LOG_CATEGORY_INFO | LOG_CATEGORY_BLUETOOTH | LOG_CATEGORY_MOTOR |
    LOG_CATEGORY_ENCODER,
};

// Macro for compile-time mask (can be redefined before including this file)
#ifndef LOG_COMPILE_MASK
#ifdef _DEBUG
#define LOG_COMPILE_MASK LOG_CATEGORY_ALL
#else
#define LOG_COMPILE_MASK LOG_CATEGORY_ALL
#endif
#endif

// Platform-specific macros for safe copying and local time
#if defined(_WIN32)
#define LOCALTIME(tm, time) localtime_s(tm, time)
#define STRNCPY_SAFE(dest, src, destSize)        \
    do {                                         \
        strncpy(dest, src, destSize - 1);        \
        dest[destSize - 1] = '\0';               \
    } while(0)
#else
#define LOCALTIME(tm, time) localtime_r(time, tm)
#define STRNCPY_SAFE(dest, src, destSize)        \
    do {                                         \
        strncpy(dest, src, destSize - 1);        \
        dest[destSize - 1] = '\0';               \
    } while(0)
#endif

// Enable inlining on all compilers
#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

// Table of category names (index = bit number)
static constexpr const char* CATEGORY_NAMES[] = {
    "ERROR",      // bit 0
    "WARNING",    // bit 1
    "INFO",       // bit 2
    "DEBUG",      // bit 3
    "MOTOR",      // bit 4
    "ENCODER",    // bit 5
    "BLUETOOTH",  // bit 6
    "PROFILE",    // bit 7
    "PERFORMANCE",// bit 8
    "COMMAND"     // bit 9
};
static_assert(sizeof(CATEGORY_NAMES) / sizeof(CATEGORY_NAMES[0]) == 10,
    "Category names array must match number of categories");

class LogManager final {
public:
    LogManager();
    ~LogManager() = default;

    // Managing categories (thread-safe)
    void setLogCategories(uint32_t categories) noexcept;
    uint32_t getLogCategories() const noexcept;
    bool isEnabled(LogCategory category) const noexcept;

    // Basic logging method (thread-safe for multiple writers)
    void logV(LogCategory category, const char* format, va_list args) noexcept;

    // Access to the buffer (for reading logs)
    int getLogCount() const noexcept;
    const char* getLogEntry(int index, LogCategory* outCategory = nullptr) const noexcept;
    void clearLog() noexcept;

private:
    struct LogEntry {
        char message[256]; 
        LogCategory category;
        std::chrono::system_clock::time_point timestamp;
    };

    static constexpr size_t MAX_ENTRIES = 10000;    // Size of the ring buffer

    std::unique_ptr<LogEntry[]> buffer = std::make_unique<LogEntry[]>(MAX_ENTRIES);
    std::atomic<size_t> writeIndex{ 0 };   // Index for entry (atomic)
    std::atomic<size_t> readIndex{ 0 };    // Index to read (atomic)
    std::atomic<uint32_t> enabledCategories{ LOG_CATEGORY_DEFAULT };
};

// Declare a global instance (defined in .cpp)
extern LogManager gLog;

template<LogCategory Category>
FORCE_INLINE void Log(const char* format, ...) noexcept {
    // Compile-time check: if the category is disabled by a mask, no code is generated
    if constexpr ((LOG_COMPILE_MASK & Category) == 0) {
        return;
    }
    else {
        // Runtime check (fast)
        if (!gLog.isEnabled(Category))
            return;

        va_list args;
        va_start(args, format);
        gLog.logV(Category, format, args);
        va_end(args);
    }
}

#define LOG_ERROR(...)      Log<LOG_CATEGORY_ERROR>(__VA_ARGS__)
#define LOG_WARNING(...)    Log<LOG_CATEGORY_WARNING>(__VA_ARGS__)
#define LOG_INFO(...)       Log<LOG_CATEGORY_INFO>(__VA_ARGS__)
#define LOG_DEBUG(...)      Log<LOG_CATEGORY_DEBUG>(__VA_ARGS__)
#define LOG_MOTOR(...)      Log<LOG_CATEGORY_MOTOR>(__VA_ARGS__)
#define LOG_ENCODER(...)    Log<LOG_CATEGORY_ENCODER>(__VA_ARGS__)
#define LOG_BLUETOOTH(...)  Log<LOG_CATEGORY_BLUETOOTH>(__VA_ARGS__)
#define LOG_PROFILE(...)    Log<LOG_CATEGORY_PROFILE>(__VA_ARGS__)
#define LOG_PERFORMANCE(...) Log<LOG_CATEGORY_PERFORMANCE>(__VA_ARGS__)
#define LOG_COMMAND(...)    Log<LOG_CATEGORY_COMMAND>(__VA_ARGS__)
