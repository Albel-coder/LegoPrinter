#include <cstdarg>
#include <string>
#include <chrono>
#include <memory>
#include <mutex>

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

class LogManager {
public:

    LogManager();
    ~LogManager() = default;

	void setLogCategories(uint32_t categories);
    uint32_t getLogCategories() const {
        return enabledCategories.load(std::memory_order_relaxed);
    }

    void addLogInternal(LogCategory category, const char* format, ...);

    int getLogCount();
    const char* getLogEntry(int index, LogCategory* outCategory = nullptr);
    int getFilteredLogCount(uint32_t categoryMask);

    void clearLog();

    FORCE_INLINE bool isCategoryEnabled(LogCategory category) const {
        return (enabledCategories.load(std::memory_order_relaxed) & category) != 0;
    }

    template<size_t N>
    FORCE_INLINE void formatToBuffer(char(&buffer)[N], const char* format, va_list args) {
        vsnprintf(buffer, N, format, args);
    }

    std::atomic<uint32_t> enabledCategories;
private:

    struct LogEntry {
        char message[1024];
        LogCategory category;
        std::chrono::system_clock::time_point timestamp;
    };

    std::unique_ptr<LogEntry[]> logBuffer;
    std::atomic<size_t> logWriteIndex{ 0 };
    std::atomic<size_t> logReadIndex{ 0 };
    std::mutex logBufferMutex;
};