#include "LogManager.h"

static constexpr size_t MAX_LOG_ENTRIES = 10000;
static constexpr size_t MAX_MESSAGE_LENGTH = 1023;

LogManager::LogManager() {
    logBuffer = std::make_unique<LogEntry[]>(MAX_LOG_ENTRIES);
    enabledCategories.store(LOG_CATEGORY_DEFAULT, std::memory_order_relaxed);
}

void LogManager::log(LogCategory category, const char* format, ...) {
    if (!isEnabled(category)) return;
    va_list args;
    va_start(args, format);
    logV(category, format, args);
    va_end(args);
}

void LogManager::logV(LogCategory category, const char* format, va_list args) {
    std::lock_guard<std::mutex> lock(logBufferMutex);
    char formatted[1024];
    vsnprintf(formatted, sizeof(formatted), format, args);
    formatted[sizeof(formatted) - 1] = '\0';

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
    STRNCPY_SAFE(logBuffer[buffer_idx].message, finalBuffer,
        sizeof(logBuffer[buffer_idx].message), MAX_MESSAGE_LENGTH);
    logBuffer[buffer_idx].category = category;
    logBuffer[buffer_idx].timestamp = now;

    logWriteIndex.store(next_write, std::memory_order_release);
}

int LogManager::getLogCount() const {
    std::lock_guard<std::mutex> lock(logBufferMutex);
    size_t writeIndex = logWriteIndex.load(std::memory_order_acquire);
    size_t readIndex = logReadIndex.load(std::memory_order_acquire);

    if (writeIndex >= readIndex) {
        size_t count = writeIndex - readIndex;
        return static_cast<int>(std::min(count, MAX_LOG_ENTRIES));
    }
    else {
        size_t count = (writeIndex + MAX_LOG_ENTRIES) - readIndex;
        return static_cast<int>(std::min(count, MAX_LOG_ENTRIES));
    }
}

const char* LogManager::getLogEntry(int index, LogCategory* outCategory) const {
    std::lock_guard<std::mutex> lock(logBufferMutex);

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

void LogManager::clearLog() {
    logWriteIndex.store(0, std::memory_order_release);
    logReadIndex.store(0, std::memory_order_relaxed);

    log(LOG_CATEGORY_INFO, "Log buffer cleared");
}

void LogManager::setLogCategories(uint32_t categories) {
    enabledCategories.store(categories, std::memory_order_relaxed);
    log(LOG_CATEGORY_INFO, "Log categories updated: 0x%08X", categories);
}

uint32_t LogManager::getLogCategories() const {
    return enabledCategories.load(std::memory_order_relaxed);
}

bool LogManager::isEnabled(LogCategory category) const {
    return (enabledCategories.load(std::memory_order_relaxed) & category) != 0;
}
