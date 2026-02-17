#include "LogManager.h"
#include <cstdio>

// Define the global object exactly once
LogManager gLog;

LogManager::LogManager() = default;

void LogManager::setLogCategories(uint32_t categories) noexcept {
    enabledCategories_.store(categories, std::memory_order_relaxed);
}

uint32_t LogManager::getLogCategories() const noexcept {
    return enabledCategories_.load(std::memory_order_relaxed);
}

bool LogManager::isEnabled(LogCategory category) const noexcept {
    return (enabledCategories_.load(std::memory_order_relaxed) & category) != 0;
}

void LogManager::logV(LogCategory category, const char* format, va_list args) noexcept {
    // Buffer for pre-formatting
    char temp[256];
    vsnprintf(temp, sizeof(temp), format, args);
    temp[sizeof(temp) - 1] = '\0';  // completion guarantee

    // Get the current time
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    tm tm_buf;
    LOCALTIME(&tm_buf, &tt);   // platform-dependent macro

    // Determine the category name using the bit index
    int bitIndex = CTTZ(category);   // category must be a power of two
    const char* catName = (bitIndex >= 0 && bitIndex < 10) ? CATEGORY_NAMES[bitIndex] : "UNKNOWN";

    // Final message with prefix
    char finalBuffer[256];
    int len = snprintf(finalBuffer, sizeof(finalBuffer),
        "[%s][%02d:%02d:%02d.%03lld] %s",
        catName,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        (long long)ms.count(),
        temp);
    if (len <= 0) return;   // formatting error

    // Reserve a slot in the ring buffer (atomically)
    size_t writeIdx = writeIndex_.fetch_add(1, std::memory_order_acq_rel);
    size_t idx = writeIdx % MAX_ENTRIES;

    // Copy data to the slot
    STRNCPY_SAFE(buffer_[idx].message, finalBuffer, sizeof(buffer_[idx].message));
    buffer_[idx].category = category;
    buffer_[idx].timestamp = now;

    // Update readIndex if the buffer is full
    size_t readIdx = readIndex_.load(std::memory_order_acquire);
    if (writeIdx - readIdx >= MAX_ENTRIES) {
        // Shift readIndex forward to "lose" the oldest entry
        readIndex_.store(writeIdx - MAX_ENTRIES + 1, std::memory_order_release);
    }
}

int LogManager::getLogCount() const noexcept {
    size_t writeIdx = writeIndex_.load(std::memory_order_acquire);
    size_t readIdx = readIndex_.load(std::memory_order_acquire);
    size_t count = writeIdx - readIdx;
    return static_cast<int>(count > MAX_ENTRIES ? MAX_ENTRIES : count);
}

const char* LogManager::getLogEntry(int index, LogCategory* outCategory) const noexcept {
    size_t readIdx = readIndex_.load(std::memory_order_acquire);
    size_t writeIdx = writeIndex_.load(std::memory_order_acquire);
    size_t available = writeIdx - readIdx;
    if (available > MAX_ENTRIES) available = MAX_ENTRIES;

    if (index < 0 || static_cast<size_t>(index) >= available)
        return "";

    size_t idx = (readIdx + index) % MAX_ENTRIES;
    if (outCategory)
        *outCategory = buffer_[idx].category;
    return buffer_[idx].message;
}

void LogManager::clearLog() noexcept {
    // Reset indexes (old records become unavailable)
    writeIndex_.store(0, std::memory_order_release);
    readIndex_.store(0, std::memory_order_release);
}
