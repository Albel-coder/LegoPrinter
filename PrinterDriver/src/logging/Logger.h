#pragma once

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

    // Manage categories
    void setEnabledCategories(uint32_t categories);
    uint32_t getEnabledCategories() const;
    bool isCategoryEnabled(LogCategory category) const;

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

    std::atomic<uint32_t> enabledCategories_{LOG_CATEGORY_RELEASE};

    void addLogInternal(LogCategory category, const char* format, ...);
};