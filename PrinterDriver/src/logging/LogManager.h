#include "ILogger.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <atomic>

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

class LogManager : public ILogger {
public:
    LogManager();
    ~LogManager() = default;

    // ILogger implementation
    void log(LogCategory category, const char* format, ...);
    void logV(LogCategory category, const char* format, va_list args) override;
    bool isEnabled(LogCategory category) const override;

    void clearLog();
    int getLogCount() const;
    const char* getLogEntry(int index, LogCategory* outCategory = nullptr) const;
    
    void setLogCategories(uint32_t categories);
    uint32_t getLogCategories() const;

private:
    struct LogEntry {
        char message[1024];
        LogCategory category;
        std::chrono::system_clock::time_point timestamp;
    };

    std::unique_ptr<LogEntry[]> logBuffer;
    std::atomic<size_t> logWriteIndex{ 0 };
    std::atomic<size_t> logReadIndex{ 0 };
    std::atomic<uint32_t> enabledCategories;
    std::mutex logBufferMutex;

    static constexpr size_t MAX_LOG_ENTRIES = 10000;
};