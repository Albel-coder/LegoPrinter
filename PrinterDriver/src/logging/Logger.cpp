#include "Logger.h"
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
    #define LOCALTIME(tm, time) localtime_s(tm, time)
    #define STRCPY_SAFE(dest, src, size) strcpy_s(dest, size, src)
    #define VSNPRINTF(buffer, size, format, args) vsnprintf_s(buffer, size, _TRUNCATE, format, args)
#else
    #define LOCALTIME(tm, time) localtime_r(time, tm)
    #define STRCPY_SAFE(dest, src, size) strncpy(dest, src, size)
    #define VSNPRINTF(buffer, size, format, args) vsnprintf(buffer, size, format, args)
#endif

#ifdef _DEBUG
#define LOG_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#else
#define LOG_ENABLED 1
#define LOG_DEBUG_ENABLED 0
#endif

#define LOG_ERROR(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_ERROR)) \
        addLogInternal(LOG_CATEGORY_ERROR, format, ##__VA_ARGS__)

#define LOG_WARNING(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_WARNING)) \
        addLogInternal(LOG_CATEGORY_WARNING, format, ##__VA_ARGS__)

#define LOG_INFO(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_INFO)) \
        addLogInternal(LOG_CATEGORY_INFO, format, ##__VA_ARGS__)

#define LOG_DEBUG(format, ...) \
    if (LOG_DEBUG_ENABLED && isCategoryEnabled(LOG_CATEGORY_DEBUG)) \
        addLogInternal(LOG_CATEGORY_DEBUG, format, ##__VA_ARGS__)

#define LOG_MOTOR(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_MOTOR)) \
        addLogInternal(LOG_CATEGORY_MOTOR, format, ##__VA_ARGS__)

#define LOG_ENCODER(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_ENCODER)) \
        addLogInternal(LOG_CATEGORY_ENCODER, format, ##__VA_ARGS__)

#define LOG_BLUETOOTH(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_BLUETOOTH)) \
        addLogInternal(LOG_CATEGORY_BLUETOOTH, format, ##__VA_ARGS__)

#define LOG_PROFILE(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_PROFILE)) \
        addLogInternal(LOG_CATEGORY_PROFILE, format, ##__VA_ARGS__)

#define LOG_PERFORMANCE(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_PERFORMANCE)) \
        addLogInternal(LOG_CATEGORY_PERFORMANCE, format, ##__VA_ARGS__)

#define LOG_COMMAND(format, ...) \
    if (isCategoryEnabled(LOG_CATEGORY_COMMAND)) \
        addLogInternal(LOG_CATEGORY_COMMAND, format, ##__VA_ARGS__)

#ifdef _DEBUG
#define LOG_PERFORMANCE_START() auto performanceStartTime = std::chrono::high_resolution_clock::now()
#define LOG_PERFORMANCE_END(category, operation) \
        auto performanceEndTime = std::chrono::high_resolution_clock::now(); \
        auto performanceDuration = std::chrono::duration_cast<std::chrono::microseconds>(performanceEndTime - performanceStartTime); \
        if (isCategoryEnabled(LOG_CATEGORY_PERFORMANCE)) \
            addLogInternal(LOG_CATEGORY_PERFORMANCE, "%s took %lld µs", operation, performanceDuration.count())
#else
#define LOG_PERFORMANCE_START() 
#define LOG_PERFORMANCE_END(category, operation)
#endif

// Singleton implementation
Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    logBuffer = std::make_unique<LogEntry[]>(MAX_LOG_ENTRIES);
}

void Logger::addLogInternal(LogCategory category, const char* format, ...) {
    if (!isCategoryEnabled(category)) {
        return;
    }

    char formatted[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(formatted, sizeof(formatted), format, args);
    formatted[sizeof(formatted) - 1] = '\0';
    va_end(args);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    tm time_info;
    LOCALTIME(&time_info, &time_t_now);

    const char* categoryName = "UNKNOWN";
    switch (category) {
    case LOG_CATEGORY_ERROR:
        categoryName = "ERROR";
        break;
    case LOG_CATEGORY_WARNING:
        categoryName = "WARNING";
        break;
    case LOG_CATEGORY_INFO:
        categoryName = "INFO";
        break;
    case LOG_CATEGORY_DEBUG:
        categoryName = "DEBUG";
        break;
    case LOG_CATEGORY_MOTOR:
        categoryName = "MOTOR";
        break;
    case LOG_CATEGORY_ENCODER:
        categoryName = "ENCODER";
        break;
    case LOG_CATEGORY_BLUETOOTH:
        categoryName = "BLUETOOTH";
        break;
    case LOG_CATEGORY_PROFILE:
        categoryName = "PROFILE";
        break;
    case LOG_CATEGORY_PERFORMANCE:
        categoryName = "PERFORMANCE";
        break;
    case LOG_CATEGORY_COMMAND:
        categoryName = "COMMAND";
        break;
    }

    char finalBuffer[1024];
    snprintf(finalBuffer, sizeof(finalBuffer),
        "[%s][%02d:%02d:%02d.%03d] %s",
        categoryName,
        time_info.tm_hour, time_info.tm_min, time_info.tm_sec,
        (int)milliseconds.count(),
        formatted);

    size_t write_idx = logWriteIndex.load(std::memory_order_relaxed);
    size_t read_idx = logReadIndex.load(std::memory_order_relaxed);

    size_t next_write = (write_idx + 1) % MAX_LOG_ENTRIES;

    if (next_write == read_idx % MAX_LOG_ENTRIES) {
        logReadIndex.store((read_idx + 1) % MAX_LOG_ENTRIES,
            std::memory_order_relaxed);
    }

    size_t buffer_idx = write_idx % MAX_LOG_ENTRIES;
    STRCPY_SAFE(logBuffer[buffer_idx].message, finalBuffer, sizeof(logBuffer[buffer_idx].message), MAX_MESSAGE_LENGTH);
    logBuffer[buffer_idx].category = category;
    logBuffer[buffer_idx].timestamp = now;

    logWriteIndex.store(next_write, std::memory_order_release);
}

// Manage categories
void Logger::setEnabledCategories(uint32_t categories) {
    enabledCategories_.store(categories);

    // Logging changes in categories
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Log categories updated: 0x%08X", categories);
    addLogInternal(LOG_CATEGORY_INFO, buffer);
}

uint32_t Logger::getEnabledCategories() const {
    return enabledCategories_.load();
}

bool Logger::isCategoryEnabled(LogCategory category) const {
    return (enabledCategories_.load() & category) != 0;
}

// Working with the log buffer
int Logger::getLogCount() const {
    size_t writeIndex = logWriteIndex.load();
    size_t readIndex = logReadIndex.load();

    if (writeIndex >= readIndex) {
        return static_cast<int>(writeIndex - readIndex);
    }
    else {
        return static_cast<int>((writeIndex + MAX_LOG_ENTRIES) - readIndex);
    }
}

const char* Logger::getLogEntry(int index, LogCategory* outCategory = nullptr) {
    size_t readIndex = logReadIndex.load(std::memory_order_acquire);
    size_t writeIndex = logWriteIndex.load(std::memory_order_acquire);

    size_t available;
    if (writeIndex >= readIndex) {
        available = writeIndex - readIndex;
    }
    else {
        available = (writeIndex + MAX_LOG_ENTRIES) - readIndex;
    }

    available = std::min(available, MAX_LOG_ENTRIES);

    if (index < 0 || static_cast<size_t>(index) >= available) {
        return "";
    }

    size_t bufferIndex = (readIndex + index) % MAX_LOG_ENTRIES;

    if (outCategory) {
        *outCategory = logBuffer[bufferIndex].category;
    }

    return logBuffer[bufferIndex].message;
}

void Logger::clearLog() {
    std::lock_guard<std::mutex> lock(logBufferMutex);
    logWriteIndex.store(0);
    logReadIndex.store(0);

    addLogInternal(LOG_CATEGORY_INFO, "Log buffer cleared");
}

// Helper methods
std::string Logger::formatTime(const std::chrono::system_clock::time_point& time) {
    auto time_t_now = std::chrono::system_clock::to_time_t(time);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()) % 1000;

    tm time_info;
    LOCALTIME(&time_info, &time_t_now);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << time_info.tm_hour << ":"
        << std::setw(2) << time_info.tm_min << ":"
        << std::setw(2) << time_info.tm_sec << "."
        << std::setw(3) << milliseconds.count();

    return oss.str();
}

const char* Logger::categoryToString(LogCategory category) {
    switch (category) {
    case LOG_CATEGORY_ERROR:       
        return "ERROR";
    case LOG_CATEGORY_WARNING:     
        return "WARNING";
    case LOG_CATEGORY_INFO:        
        return "INFO";
    case LOG_CATEGORY_DEBUG:       
        return "DEBUG";
    case LOG_CATEGORY_MOTOR:       
        return "MOTOR";
    case LOG_CATEGORY_ENCODER:     
        return "ENCODER";
    case LOG_CATEGORY_BLUETOOTH:   
        return "BLUETOOTH";
    case LOG_CATEGORY_PROFILE:     
        return "PROFILE";
    case LOG_CATEGORY_PERFORMANCE: 
        return "PERFORMANCE";
    case LOG_CATEGORY_COMMAND:     
        return "COMMAND";
    default:                       
        return "UNKNOWN";
    }
}
