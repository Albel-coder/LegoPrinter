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

#include "LegoPrinterCore.h"
#include <simpleble/SimpleBLE.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <queue>
#include <condition_variable>
#include <algorithm>
#include <cstring>
#include <cstdarg>
#include <iomanip>
#include <sstream>
#include <cmath>

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

using namespace std::chrono_literals;

// Constants for working with LEGO HUB
const std::string LEGO_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
const std::string LEGO_HUB_CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123";

enum LogCategory {
    LOG_CATEGORY_NONE        = 0,
    LOG_CATEGORY_ERROR       = 1 << 0,
    LOG_CATEGORY_WARNING     = 1 << 1,
    LOG_CATEGORY_INFO        = 1 << 2,
    LOG_CATEGORY_DEBUG       = 1 << 3,
    LOG_CATEGORY_MOTOR       = 1 << 4,
    LOG_CATEGORY_ENCODER     = 1 << 5,
    LOG_CATEGORY_BLUETOOTH   = 1 << 6,
    LOG_CATEGORY_PROFILE     = 1 << 7,
    LOG_CATEGORY_PERFORMANCE = 1 << 8,
    LOG_CATEGORY_COMMAND     = 1 << 9,

    LOG_CATEGORY_ALL        = 0xFFFFFFFF,
    LOG_CATEGORY_DEFAULT    = LOG_CATEGORY_ERROR | LOG_CATEGORY_WARNING |
                              LOG_CATEGORY_INFO | LOG_CATEGORY_MOTOR |
                              LOG_CATEGORY_ENCODER,

#ifdef _DEBUG
    LOG_CATEGORY_RELEASE = LOG_CATEGORY_ALL,
#else
    LOG_CATEGORY_RELEASE = LOG_CATEGORY_ERROR | LOG_CATEGORY_WARNING | LOG_CATEGORY_INFO,
#endif
};

// --Printer implementation class--
// Internal driver implementation, hidden from the outside world
// Contains all the core logic for working with Bluetooth and the LEGO HUB
class PrinterImplementation {
public:
    IPrinter interface;

private:

    // Printer Status
    SimpleBLE::Peripheral peripheral;
    std::string lastError;
    std::atomic<bool> operationInProgress;
    std::atomic<bool> stopRequested;
    std::atomic<bool> isValid;
    std::atomic<int> status;
    std::atomic<bool> wasConnected{ false };

    struct MotorState {
        std::atomic<double> absolutePosition{ 0.0 };
        std::atomic<double> segmentAccumulator{ 0.0 };
        std::atomic<double> relativePosition{ 0.0 };
        std::atomic<double> currentPosition{ 0.0 };
        std::atomic<double> currentSpeed{ 0.0 };
        std::atomic<bool> isMoving{ false };
        std::atomic<bool> threadRunning{ false };

        std::atomic<double> segmentTarget{ 0.0 };

        std::atomic<double> segmentStartPosition{ 0.0 };
        std::atomic<double> segmentStartRelative{ 0.0 };
        std::atomic<double> segmentTargetDistance{ 0.0 };
        std::atomic<double> currentSegmentTraveled{ 0.0 };
        std::atomic<int> currentSegmentIndex{ 0 };
        std::atomic<bool> profileActive{ false };

        std::vector<SpeedProfilePoint> activeProfile;
        int profileTimeoutMs = 60000;

        double lastAbsolutePosition{ 0.0 };
        std::mutex positionUpdateMutex;

        // Default constructor
        MotorState() = default;

        // Move constructor
        MotorState(MotorState&& other) noexcept
            : currentPosition(other.currentPosition.load())
            , relativePosition(other.relativePosition.load())
            , segmentStartPosition(other.segmentStartPosition.load())
            , currentSegmentTraveled(other.currentSegmentTraveled.load())
            , segmentStartRelative(other.segmentStartRelative.load())
            , currentSegmentIndex(other.currentSegmentIndex.load())
            , threadRunning(other.threadRunning.load())
            , profileActive(other.profileActive.load())
            , activeProfile(std::move(other.activeProfile))
            , profileTimeoutMs(other.profileTimeoutMs)
        {
        }

        // Move assignment
        MotorState& operator=(MotorState&& other) noexcept {
            if (this != &other) {
                currentPosition.store(other.currentPosition.load());
                relativePosition.store(other.relativePosition.load());
                segmentStartPosition.store(other.segmentStartPosition.load());
                currentSegmentTraveled.store(other.currentSegmentTraveled.load());
                segmentStartRelative.store(other.segmentStartRelative.load());
                currentSegmentIndex.store(other.currentSegmentIndex.load());
                threadRunning.store(other.threadRunning.load());
                profileActive.store(other.profileActive.load());
                activeProfile = std::move(other.activeProfile);
                profileTimeoutMs = other.profileTimeoutMs;
            }
            return *this;
        }

        // Remove copy
        MotorState(const MotorState&) = delete;
        MotorState& operator=(const MotorState&) = delete;
    };

    std::mutex sendCommandMutex;

    std::map<uint8_t, MotorState> motorStates;
    std::map<uint8_t, std::thread> motorThreads;
    std::condition_variable motorStatesCV;

    // Simple synchronization system
    std::mutex operationMutex;
    std::mutex completionMutex;
    std::condition_variable completionCV;

    struct CommandExecution {
        std::atomic<bool> completed{ false };
        std::atomic<bool> waiting{ false };

        CommandExecution() {
            completed = true;
            waiting = false;
        }

        CommandExecution(const CommandExecution&) = delete;
        CommandExecution& operator=(const CommandExecution&) = delete;

        CommandExecution(CommandExecution&& Other) noexcept
            : completed(Other.completed.load()), waiting(Other.waiting.load()) {
        }
    };

    std::map<uint8_t, CommandExecution> commandStatus;

    struct SpeedControlState {
        std::atomic<bool> active{ false };
        std::atomic<size_t> currentPointIndex{ 0 };
        std::thread controlThread;

        std::vector<SpeedProfilePoint> profilePoints;
        int timeoutMs;
    };

    std::map<uint8_t, SpeedControlState> speedControlStates;
    std::mutex speedControlMutex;

    std::mutex motorStatesMutex;

    std::atomic<uint8_t> batteryLevel{ 0 };
    std::chrono::steady_clock::time_point lastBatteryUpdate;

    std::atomic<uint32_t> enabledCategories;

    struct LogEntry {
        char message[1024];
        LogCategory category;
        std::chrono::system_clock::time_point timestamp;
    };

    static constexpr size_t MAX_LOG_ENTRIES = 10000;
    static constexpr size_t MAX_MESSAGE_LENGTH = 1023;

    std::unique_ptr<LogEntry[]> logBuffer;
    std::atomic<size_t> logWriteIndex{ 0 };
    std::atomic<size_t> logReadIndex{ 0 };
    std::mutex logBufferMutex;

public:

    PrinterImplementation() :
        operationInProgress(false),
        stopRequested(false),
        isValid(true),
        batteryLevel(0),
        lastBatteryUpdate(std::chrono::steady_clock::now()),
        status(0) {

        logBuffer = std::make_unique<LogEntry[]>(MAX_LOG_ENTRIES);
        enabledCategories.store(LOG_CATEGORY_RELEASE, std::memory_order_relaxed);

        LOG_INFO("PrinterImplementation created");
    }

    ~PrinterImplementation() {
        try {
            // Stop all port flows
            for (auto& [port, running] : portThreadRunning) {
                running = false;
                portQueueCVs[port].notify_all();
            }

            for (auto& [port, thread] : portThreads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }

            std::lock_guard<std::mutex> lock(completionMutex);
            completionCV.notify_all();

            for (auto& [port, state] : speedControlStates) {
                state.active = false;
                if (state.controlThread.joinable()) {
                    state.controlThread.join();
                }
            }
        }
        catch (...) {
            // Ignore condition variable errors
        }

        // Automatic shutdown when variable is destroyed
        if (wasConnected) {
            isValid = false;
            stopRequested = true;

            // Stop all motor threads
            for (auto& [port, State] : motorStates) {
                State.threadRunning = false;
                State.profileActive = false;
            }

            for (auto& [port, Thread] : motorThreads) {
                if (Thread.joinable()) {
                    Thread.join();
                }
            }

            if (peripheral.is_connected()) {
                try {
                    if (!peripheral.address().empty() && peripheral.is_connected()) {
                        peripheral.disconnect();
                    }
                }
                catch (const std::exception& ex) {
                    // Ignore all errors
                }
            }
        }
    }

    void setLogCategories(uint32_t categories) {
        enabledCategories.store(categories, std::memory_order_relaxed);
        addLogInternal(LOG_CATEGORY_INFO, "Log categories updated: 0x%08X", categories);
    }

    uint32_t getLogCategories() const {
        return enabledCategories.load(std::memory_order_relaxed);
    }

private:    

    FORCE_INLINE bool isCategoryEnabled(LogCategory category) const {
        return (enabledCategories.load(std::memory_order_relaxed) & category) != 0;
    }

    template<size_t N>
    FORCE_INLINE void formatToBuffer(char(&buffer)[N], const char* format, va_list args) {
        vsnprintf(buffer, N, format, args);
    }

    void addLogInternal(LogCategory category, const char* format, ...) {
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
        STRNCPY_SAFE(logBuffer[buffer_idx].message, finalBuffer, 
             sizeof(logBuffer[buffer_idx].message), MAX_MESSAGE_LENGTH);
        logBuffer[buffer_idx].category = category;
        logBuffer[buffer_idx].timestamp = now;

        logWriteIndex.store(next_write, std::memory_order_release);
    }

    void setMotorCalibration(uint8_t port, double factor) {
        motorCalibrationFactors[port] = factor;
        LOG_DEBUG("Set calibration for port 0x%02X: %.3f", port, factor);
    }

    // Set notification handler
    void setupNotificationHandler() {
        try {
            if (!peripheral.is_connected()) {
                LOG_ERROR("ERROR: Peripheral not connected for encoder notifications");
                return;
            }

            peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                [this](const std::vector<uint8_t>& Data) {
                    this->handleHubNotification(Data);
                });

            LOG_INFO("Encoder notifications setup completed - SUBSCRIBED");
        }
        catch (const std::exception& ex) {
            LOG_INFO("Error setting up encoder notifications: %s", ex.what());
        }
    }

    void handleHubNotification(const std::vector<uint8_t>& data) {
        if (!isValid || data.empty()) return;

        const uint8_t messageType = data[2];

        // Detailed logging of ALL messages
        if (isCategoryEnabled(LOG_CATEGORY_DEBUG)) {
            std::string hex;
            for (size_t i = 0; i < std::min(data.size(), (size_t)10); i++) {
                char buf[4];
                snprintf(buf, sizeof(buf), "%02X ", data[i]);
                hex += buf;
            }
            LOG_DEBUG("NOTIFICATION: Type=0x%02X, Data: %s", messageType, hex.c_str());
        }

        switch (messageType) {
        case 0x01: // Hub Properties - BATTERY!
            handleHubProperties(data);
            break;

        case 0x04: // System Command Reply
            if (data.size() > 5 && data[0] > 0x03) {
                handleSystemCommandReply(data);
            }
            else {
                const uint8_t port = data[3];
                handleAbsoluteEncoder(port, data);
            }
            break;

        case 0x05: // Port Information
            handlePortInformation(data);
            break;

        case 0x45: // Incremental encoder
            if (data.size() >= 5) {
                const uint8_t port = data[3];
                handleIncrementalEncoder(port, data);
            }
            break;

        case 0x82: // Operation completion command
            if (data.size() >= 5) {
                const uint8_t port = data[3];
                handleCommandCompletion(port, data);
            }
            break;

        default:
            LOG_DEBUG("Unhandled notification type: 0x%02X", messageType);
            break;
        }
    }

    void handleHubProperties(const std::vector<uint8_t>& data) {
        if (data.size() < 6) {
            LOG_ERROR("Hub Properties message too short: %zu bytes", data.size());
            return;
        }

        const uint8_t property = data[3];
        const uint8_t operation = data[4];

        LOG_DEBUG("Hub Property: property=0x%02X, operation=0x%02X", property, operation);

        if (property == 0x06) { // Battery Level Property
            if (operation == 0x06) { // Update operation
                if (data.size() >= 6) {
                    const uint8_t batteryValue = data[5];

                    // Check the correctness of the value
                    if (batteryValue <= 100) {
                        batteryLevel.store(batteryValue);
                        lastBatteryUpdate = std::chrono::steady_clock::now();

                        LOG_INFO("Battery level updated from Hub Property: %u%%", batteryValue);

                        // Log warnings
                        if (batteryValue < 21) {
                            LOG_WARNING("WARNING: Low battery! %u%%", batteryValue);
                        }
                        else if (batteryValue == 100) {
                            LOG_INFO("Battery fully charged");
                        }
                    }
                    else {
                        LOG_WARNING("Invalid battery value received: %u%% (capping to 100)", batteryValue);
                        batteryLevel.store(100);
                        lastBatteryUpdate = std::chrono::steady_clock::now();
                    }
                }
                else {
                    LOG_ERROR("Battery property message too short");
                }
            }
            else {
                LOG_DEBUG("Other battery operation: 0x%02X", operation);
            }
        }
        else {
            // Other hub properties
            LOG_DEBUG("Other hub property: 0x%02X", property);
        }
    }

    void handlePortInformation(const std::vector<uint8_t>& data) {
        if (data.size() < 5) {
            LOG_ERROR("Port Information message too short: %zu bytes", data.size());
            return;
        }

        const uint8_t port = data[3];
        const uint8_t infoType = data[4];

        LOG_DEBUG("Port Information: port=0x%02X, infoType=0x%02X", port, infoType);

        // There might also be information about the battery or port status here.
        // But in the logs we only see 05 00 05 02 05
        // 02 is the port, 05 is the information type.
    }

    void handleIncrementalEncoder(uint8_t port, const std::vector<uint8_t> data) {
        if (data.size() < 5) return;

        initializeMotorState(port);
        auto& state = motorStates[port];

        uint8_t positionByte = data[4];
        int8_t signedPosition = static_cast<int8_t>(positionByte);
        double positionDelta = static_cast<double>(signedPosition) * (1.0 / 360.0);

        if (state.profileActive) {
            updateMotorPosition(port, positionDelta);
        }

        if (std::abs(positionDelta) > 0.001) {
            LOG_ENCODER("ENCODER 0x45: Port=0x%02X, Raw=%d, Delta=%.4f rev",
                port, signedPosition, positionDelta);
        }
    }

    void handleAbsoluteEncoder(uint8_t port, const std::vector<uint8_t> data) {
        if (data.size() < 8) return;

        initializeMotorState(port);
        auto& state = motorStates[port];

        int32_t PositionRaw = (static_cast<int32_t>(data[4])) |
            (static_cast<int32_t>(data[5]) << 8) |
            (static_cast<int32_t>(data[6]) << 16) |
            (static_cast<int32_t>(data[7]) << 24);

        int32_t SignedPosition = static_cast<int32_t>(PositionRaw);

        // Conversion for absolute encoder
        double absolutePosition = static_cast<double>(SignedPosition) / 360.0;

        // For an absolute encoder, you need to calculate the delta from the previous value
        double positionDelta = 0.0;

        {
            std::lock_guard<std::mutex> lock(state.positionUpdateMutex);

            if (state.profileActive) {
                positionDelta = absolutePosition - state.lastAbsolutePosition;

                if (positionDelta > 180.0) positionDelta -= 360.0;
                else if (positionDelta < -180.0) positionDelta += 360.0;
            }

            state.lastAbsolutePosition = absolutePosition;
        }

        if (state.profileActive) {
            updateMotorPosition(port, positionDelta);
        }

        if (std::abs(positionDelta) > 0.001) {
            LOG_ENCODER("ENCODER 0x04: Port=0x%02X, Raw=%d, Abs=%.3f, Delta=%.4f rev",
                port, SignedPosition, absolutePosition, positionDelta);
        }
    }

    void handleCommandCompletion(uint8_t port, const std::vector<uint8_t> data) {
        if (data.size() < 5) return;

        uint8_t feedback = data[4];

        // Command ends
        if (feedback == 0x0A)
        {
            std::lock_guard<std::mutex> lock(completionMutex);

            if (commandStatus[port].waiting && !commandStatus[port].completed)
            {
                commandStatus[port].completed = true;
                completionCV.notify_all();
            }
        }
    }

    const double EMPIRICAL_CALIBRATION = 3.0 / 2.0;

    // In the UpdateMotorPosition function:
    void updateMotorPosition(uint8_t port, double positionDelta) {
        auto& state = motorStates[port];

        if (!state.profileActive.load(std::memory_order_relaxed)) {
            if (std::abs(positionDelta) > 0.001) {
                LOG_ENCODER("IGNORED POS_UPDATE (profile inactive): Port=0x%02X, Delta=%4.f", port, positionDelta);
            }

            return;
        }

        std::lock_guard<std::mutex> lock(state.positionUpdateMutex);

        // Apply calibration
        double calibration = motorCalibrationFactors.count(port)
            ? motorCalibrationFactors[port]
            : EMPIRICAL_CALIBRATION;

        double calibratedDelta = positionDelta * calibration;

        // Update the absolute position
        double currentAbs = state.absolutePosition.load(std::memory_order_relaxed);
        double newAbs = currentAbs + calibratedDelta;
        state.absolutePosition.store(newAbs, std::memory_order_relaxed);

        // Update segment accumulation ONLY if the profile is active
        if (state.profileActive) {
            double currentSeg = state.segmentAccumulator.load(std::memory_order_relaxed);
            double newSeg = currentSeg + calibratedDelta;
            state.segmentAccumulator.store(newSeg, std::memory_order_relaxed);
        }

        if (std::abs(calibratedDelta) > 0.001) {
            LOG_ENCODER("POS_UPDATE: Port=0x%02X, RawDelta=%.4f, CalDelta=%.4f, NewAbs=%.3f, NewSeg=%.3f",
                port, positionDelta, calibratedDelta, newAbs,
                state.profileActive ? state.segmentAccumulator.load() : 0.0);
        }
    }   

    void sendCommandVector(std::vector<uint8_t> command) {
        std::lock_guard<std::mutex> Lock(sendCommandMutex);

        if (!isValid) {
            LOG_ERROR("SendCommandVector: Printer implementation is not valid");
            return;
        }
        if (!peripheral.is_connected()) {
            LOG_ERROR("SendCommandVector: Printer is not connect");
            return;
        }

        try {
            // Check connection
            if (!peripheral.is_connected()) {
                LOG_ERROR("SendCommandVector: Peripheral not connected");
                return;
            }

            // Logging sending command
            std::string hexCommand = "Command bytes: ";
            for (auto byte : command) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X", byte);
                hexCommand += hex;
            }
            LOG_COMMAND("%s", hexCommand.c_str());

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, command);
            LOG_COMMAND("Command sent successfully!");
        }
        catch (const std::exception& e) {
            LOG_ERROR("Error sending command: %s", e.what());
            lastError = e.what();
        }
    }

public:

    // Access to log from C-interface
    int getLogCount() {
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

    const char* getLogEntry(int index, LogCategory* outCategory = nullptr) {
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

    int getFilterLogCount(uint32_t categoryMask) {
        size_t writeIndex = logWriteIndex.load(std::memory_order_acquire);
        size_t readIndex = logReadIndex.load(std::memory_order_relaxed);

        int count = 0;

        std::lock_guard<std::mutex> lock(logBufferMutex);        

        for (size_t i = readIndex; i < writeIndex; i++) {
            size_t index = i % MAX_LOG_ENTRIES;
            if (logBuffer[index].category & categoryMask) {
                count++;
            }
        }

        return count;
    }

    void clearLog() {
        logWriteIndex.store(0, std::memory_order_release);
        logReadIndex.store(0, std::memory_order_relaxed);

        addLogInternal(LOG_CATEGORY_INFO, "Log buffer cleared");
    }

    const char* getLastErrorMessage() {
        return lastError.empty() ? "" : lastError.c_str();
    }

    // Basic methods
    bool connect() {
        if (!isValid) return false;

        std::lock_guard<std::mutex> lock(operationMutex);

        try {
            // Checking Bluetooth Status
            LOG_BLUETOOTH("Checking Bluetooth status:");

            bool bleEnabled = SimpleBLE::Adapter::bluetooth_enabled();
            LOG_BLUETOOTH("  - SimpleBLE::Adapter::bluetooth_enabled(): %s", 
              bleEnabled ? "true" : "false");

            // Getting a list of adapters
            auto adapters = SimpleBLE::Adapter::get_adapters();
            LOG_BLUETOOTH("  - Adapters found: %zu", adapters.size());

            if (adapters.empty()) {
                LOG_ERROR("Bluetooth adapters not found! Possible reasons:");
                LOG_ERROR("1. The Bluetooth adapter is disabled or not working");
                LOG_ERROR("2. Drivers not installed");
                LOG_ERROR("3. Hardware problem");
                return false;
            }

            // We use the first adapter
            SimpleBLE::Adapter& adapter = adapters[0];
            LOG_BLUETOOTH("Adapter used: %s [%s]",
                adapter.identifier().c_str(),
                adapter.address().c_str());

            // Setting up callbacks
            adapter.set_callback_on_scan_start([]() {});

            LOG_BLUETOOTH("Scanning started...");

            adapter.set_callback_on_scan_stop([]() {});

            LOG_BLUETOOTH("Scanning stopped");

            adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral peripheral) {
                std::string name = peripheral.identifier();
                std::string address = peripheral.address();
                int rssi = peripheral.rssi();

                // Convert the name to uppercase for universality
                std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                // Check by name
                bool isLego = (name.find("LEGO") != std::string::npos) ||
                    (name.find("HUB") != std::string::npos) ||
                    (name.find("CONTROL") != std::string::npos);

                // Check with manufacturer data (LEGO Company ID: 0x0397)
                auto manufacturer_data = peripheral.manufacturer_data();
                for (const auto& data : manufacturer_data) {
                    // LEGO Company ID: 0x0397 (little-endian: 97 03)
                    if (data.first == 0x0397) {
                        isLego = true;
                        LOG_BLUETOOTH("[LEGO Manufacturer Data Found]");
                    }
                }

                if (isLego) {
                    LOG_BLUETOOTH("LEGO HUB DISCOVERED!");
                }
                });

            // Start scanning
            adapter.scan_start();
            LOG_BLUETOOTH("Starting Bluetooth scan for 10 seconds...");
            std::this_thread::sleep_for(10s);
            adapter.scan_stop();

            // We get a list of found devices
            auto peripherals = adapter.scan_get_results();
            LOG_BLUETOOTH("Scan completed. Found %d devices", peripherals.size());

            // Search LEGO Hub
            for (auto& scannedPeripheral : peripherals) {
                std::string name = scannedPeripheral.identifier();
                std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                if (name.find("LEGO") != std::string::npos ||
                    name.find("HUB") != std::string::npos ||
                    name.find("CONTROL") != std::string::npos) {

                    LOG_BLUETOOTH("Attempting to connect to LEGO Hub: %s", name.c_str());

                    // Connection attempt
                    try {
                        scannedPeripheral.connect();

                        // In the main function, after connection:               
                        if (scannedPeripheral.is_connected()) {
                            LOG_BLUETOOTH("Successfully connected to LEGO Hub");

                            peripheral = std::move(scannedPeripheral);

                            setupNotificationHandler();

                            // Looking for LEGO Hub service and features
                            SimpleBLE::Service legoService;
                            SimpleBLE::Characteristic legoChar;

                            for (auto& service : scannedPeripheral.services()) {
                                if (service.uuid() == LEGO_HUB_SERVICE_UUID) {
                                    legoService = service;
                                    for (auto& characteristic : service.characteristics()) {
                                        if (characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID) {
                                            legoChar = characteristic;
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }

                            if (legoChar.uuid().empty()) return false;

                            wasConnected = true;
                            return true;
                        }
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR("Connection error: %s", e.what());
                        lastError = e.what();
                    }
                }
            }

            LOG_ERROR("No LEGO Hub found or connection failed");
            return false;
        }
        catch (const std::exception& e) {
            LOG_ERROR("Exception in Connect: %s", e.what());
            lastError = e.what();
            return false;
        }
    }

    bool disconnect() {
        std::lock_guard<std::mutex> contextLock(operationMutex);

        // Disconnect already completed
        if (!isValid || !peripheral.initialized() || peripheral.address().empty()) return false;

        if (peripheral.is_connected()) {
            try {
                peripheral.disconnect();
            }
            catch (...) {
                // Ignoring disconnection errors
                return false;
            }
        }
        return true;
    }

    bool isConnected() {
        return peripheral.is_connected();
    }

    void printConnectionInfo() {
        std::lock_guard<std::mutex> lock(operationMutex);

        LOG_INFO("=== CONNECTION INFORMATION ===");

        if (!peripheral.is_connected()) {
            LOG_ERROR("NOT CONNECTED to any device");
            return;
        }

        LOG_INFO("Device: %s", peripheral.identifier().c_str());
        LOG_INFO("Address: %s", peripheral.address().c_str());
        LOG_INFO("RSSI: %d", peripheral.rssi());
        LOG_INFO("Connected: %s", peripheral.is_connected() ? "true" : "false");

        auto Services = peripheral.services();
        LOG_INFO("Services count: %zu", Services.size());

        for (auto Service : Services) {
            LOG_INFO("Service UUID: %s", Service.uuid().c_str());

            if (Service.uuid() == LEGO_HUB_SERVICE_UUID) {
                LOG_INFO(" >>> LEGO SERVICE FOUND!");
                for (auto Characteristic : Service.characteristics()) {
                    LOG_INFO("    Characteristic: %s", Characteristic.uuid().c_str());
                    if (Characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID) {
                        LOG_INFO("    >>> LEGO CHARACTERISTIC FOUND!");
                    }
                }
            }
        }

        LOG_INFO("========================================");
    }

    void rotateMotor(const MotorCommand* commands, int count) {
        // Check parameters
        if (!isValid || count <= 0 || !commands) {
            LOG_ERROR("RotateMotor: Invalid parameters");
            return;
        }

        LOG_INFO("RotateMotor called with %d commands", count);

        // Check connection
        if (!peripheral.is_connected()) {
            LOG_ERROR("Printer is not connected!");
            return;
        }

        std::lock_guard<std::mutex> operationLock(operationMutex);

        // Prepare command tracking
        for (int i = 0; i < count; i++) {
            commandStatus[commands[i].port].completed = false;
            commandStatus[commands[i].port].waiting = true;
        }

        // Send all commands
        for (int i = 0; i < count; i++) {
            sendSingleMotorCommand(commands[i]);
        }

        waitForCommandsCompletion(commands, count);
        LOG_INFO("RotateMotor completed");
    }

    // Monitoring
    bool isMotorMoving(unsigned char port) {
        if (motorStates.count(port)) {
            return motorStates[port].isMoving;
        }
        return false;
    }

    double getMotorPosition(uint8_t port) {
        if (motorStates.count(port)) {
            // We return AbsolutePosition since it is relevant
            return motorStates[port].absolutePosition.load();
        }
        return 0.0;
    }

    void setMotorSpeed(uint8_t port, int8_t speed)
    {
        if (!isValid || !peripheral.is_connected()) return;

        LOG_MOTOR("Setting motor speed: Port=0x%02X, Speed=%d", port, speed);

        // First command: Activate mode
        std::vector<uint8_t> setupCommand = {
            0x09,       // Package length
            0x00,       // Hub ID
            0x41,       // Port configuration command
            port,       // Motor port
            0x01,       // Mode: Power (1)
            0x00,       // Data Format
            0x00,       // Unit
            0x00,       // Range min
            0x00        // Range max
        };

        sendCommandVector(setupCommand);

        // Second Team: motor control
        std::vector<uint8_t> motorCommand = {
            0x08,       // Package length
            0x00,       // Hub ID
            0x81,       // Output control command
            port,       // Motor port
            0x02,       // Subcommand: WriteDirectModeData
            0x01,       // Mode: Power (1)
            static_cast<uint8_t>(speed) // Speed
        };

        sendCommandVector(motorCommand);
    }

private:

    void sendSingleMotorCommand(const MotorCommand& command) {
        LOG_MOTOR("Motor command - Port: 0x%02X, Speed: %d, Revolutions: %.2f",
            command.port, command.speed, command.revolutions);

        // Convert revolutions to absolute degrees (1 revolution = 360 degrees)
        int32_t degrees = static_cast<int32_t>(std::round(command.revolutions * 360.0));
        LOG_MOTOR("Calculated degrees: %d", degrees);

        // Command 1: Activate the rotation mode by angle
        std::vector<uint8_t> setupCommand = {
            0x09, // Message length
            0x00, // Hub ID
            0x41, // Port configuration command
            command.port, // Motor port
            0x02, // Mode: speed (to rotate at a certain angle)
            0x00, // Data Format
            0x01, // Unit of measurement: degrees
            0x00, // Range
            0x00  // Range
        };
        sendCommandVector(setupCommand);
        std::this_thread::sleep_for(50ms);

        // Command 2: Rotate by a given angle
        std::vector<uint8_t> payload = {
            0x0F,       // Message length (15 bytes)
            0x00,       // Message counter
            0x81,       // Output control command
            command.port, // Port or combo port
            0x11,
            0x0B,       // Sub-team
            // Rotation angle (4 bytes little-endian)
            static_cast<uint8_t>(degrees & 0xFF),
            static_cast<uint8_t>((degrees >> 8) & 0xFF),
            static_cast<uint8_t>((degrees >> 16) & 0xFF),
            static_cast<uint8_t>((degrees >> 24) & 0xFF),
            // Speed (1 byte)
            static_cast<uint8_t>(command.speed),
            // Maximum power (usually 100%)
            100,
            // Final state (0 = float/coast, 1 = brake/hold)
            0x01,       // Hold the position after completion
            // Use profile (0 = use acceleration profile)
            0x00
        };

        LOG_MOTOR("Sending motor command to port 0x%02X", command.port);
        sendCommandVector(payload);
    }

    void waitForCommandsCompletion(const MotorCommand* Commands, int count) {
        std::unique_lock<std::mutex> lock(completionMutex);

        // Wait while condition variable gets notification about completing all commands
        bool allCompleted = completionCV.wait_for(lock, std::chrono::seconds(30),
            [this, Commands, count]() {
                for (int i = 0; i < count; i++) {
                    if (!commandStatus[Commands[i].port].completed) return false;
                }
            });

        if (!allCompleted) {
            // For timeout - end all
            for (int i = 0; i < count; i++) {
                commandStatus[Commands[i].port].waiting = false;
            }
        }

        // Delete waiting status
        for (int i = 0; i < count; i++) {
            commandStatus[Commands[i].port].waiting = false;
        }
    }

    bool waitForCommandCompletion(uint8_t port, int timeoutMs = 15000) {
        std::unique_lock<std::mutex> lock(completionMutex);

        // Make sure the element exists in the map
        if (commandStatus.find(port) == commandStatus.end()) return false;

        // Setting the wait state
        commandStatus[port].completed = false;
        commandStatus[port].waiting = true;

        // We are waiting for notification of completion
        bool success = completionCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this, port]() {
                auto it = commandStatus.find(port);
                if (it != commandStatus.end()) {
                    return it->second.completed.load();
                }

                return true; // If there is no port, we consider it complete.
            });

        if (commandStatus.find(port) != commandStatus.end()) {
            commandStatus[port].waiting = false;
        }

        if (!success) {
            LOG_WARNING("WaitForCommandCompletion timeout for port 0x%02X", port);
        }
        else {
            LOG_WARNING("WaitForCommandCompletion success for port 0x%02X", port);
        }

        return success;
    }

public:

    void sendCommand(const unsigned char* command, int length) {
        if (!isValid || length < 1) return;

        std::lock_guard<std::mutex> lock(operationMutex);

        try {
            std::vector<uint8_t> commandBuffer(command, command + length);

            // Check connection
            if (!peripheral.is_connected()) {
                LOG_ERROR("Peripheral is not connected");
                return;
            }

            // Logging sending command
            std::string hexCommand = "Command bytes: ";
            for (auto byte : commandBuffer) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X", byte);
                hexCommand += hex;
            }
            LOG_COMMAND(hexCommand.c_str());

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, commandBuffer);
            LOG_COMMAND("Command sent successfully!");
        }
        catch (const std::exception& e) {
            LOG_ERROR("Error sending command: %s", e.what());
            lastError = e.what();
        }
    }

    void safeShutdown() {
        isValid = false;
        stopRequested = true;

        // Safe breaking all operations
        try {
            std::lock_guard<std::mutex> Lock(completionMutex);
            completionCV.notify_all();
        }
        catch (...) {
            // Ignoring all errors
        }
    }

    //----- Execute speed profile methods -----

public:

    bool executeSpeedProfile(const SpeedProfile* profile)
    {
        if (!isValid || !profile || profile->count < 1) {
            LOG_ERROR("Error: Invalid profile parameters");
            return false;
        }

        LOG_PROFILE("=== EXECUTE SPEED PROFILE ===");
        LOG_PROFILE("Port: 0x%02X, Segments: %d, Timeout: %d ms",
            profile->port, profile->count, profile->timeoutMs);

        uint8_t port = profile->port;

        // Copying profile data for use in lambda
        std::vector<SpeedProfilePoint> profilePoints;
        for (int i = 0; i < profile->count; i++) {
            profilePoints.push_back(profile->points[i]);
        }
        int timeoutMs = profile->timeoutMs;

        // Adding a task to the port queue
        enqueuePortCommand(port, [this, port, profilePoints, timeoutMs]() {
            this->executeSpeedProfileInternal(port, profilePoints, timeoutMs);
        });

        return true;
    }

    bool executeSpeedProfiles(const SpeedProfile* profiles, int count) {
        if (!isValid || !profiles || count < 1) {
            LOG_ERROR("Error: Invalid parameters for executeSpeedProfiles");
            return false;
        }

        LOG_PROFILE("=== Execution multiple speed profiles ===");
        LOG_PROFILE("Number of profiles: %d", count);

        std::set<uint8_t> usedPorts;
        for (int i = 0; i < count; i++) {
            if (usedPorts.count(profiles[i].port)) {
                LOG_ERROR("Error: duplicate port 0x%02X in profiles", profiles[i].port);
                return false;
            }
            usedPorts.insert(profiles[i].port);

            LOG_PROFILE("Profile %d: Port=0x%02X, Segments=%d, Timeout=%dms", i, profiles[i].port, profiles[i].count, profiles[i].timeoutMs);
        }

        stopAllProfiles();

        for (int i = 0; i < count; i++) {
            startProfileExecution(profiles[i].port, profiles[i]);
        }

        std::this_thread::sleep_for(100ms);

        int maxTimeoutMs = 0;
        for (int i = 0; i < count; i++) {
            if (profiles[i].timeoutMs > maxTimeoutMs) {
                maxTimeoutMs = profiles[i].timeoutMs;
            }
        }

        waitForAllProfiles(maxTimeoutMs + 10000);

        bool allSuccessful = true;
        {
            std::lock_guard<std::mutex> lock(profileExecutionsMutex);
            for (auto& [port, execution] : profileExecutions) {
                if (!execution.completed) {
                    allSuccessful = false;
                    LOG_PROFILE("Profile for port 0x%02X did not complete ", port);
                }
            }
        }

        for (int i = 0; i < count; i++) {
            setMotorSpeed(profiles[i].port, 0);
        }

        LOG_PROFILE("Multiple profiles execution %s", allSuccessful ? "success" : "failed");
        return allSuccessful;
    }

private:

    void executeSpeedProfileInternal(uint8_t port, const std::vector<SpeedProfilePoint>& profilePoints, int timeoutMs) {
        LOG_PROFILE("=== Starting speed profile execution for port 0x%02X ===", port);

        // Activate the encoder
        activateEncoderMode(port);
        std::this_thread::sleep_for(200ms);

        // Setting up notifications
        setupNotificationHandler();
        std::this_thread::sleep_for(200ms);

        // Launching the profile controller
        if (!startRelativeProfileController(port, profilePoints, timeoutMs)) {
            LOG_ERROR("Failed to start profile controller for port 0x%02X", port);
        }

        // We are waiting for the profile to be completed
        auto& state = motorStates[port];
        auto startTime = std::chrono::steady_clock::now();

        while (state.profileActive && isValid && portThreadRunning[port]) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

            if (elapsed.count() > timeoutMs) {
                LOG_WARNING("Profile execution timeout for port 0x%02X", port);
                break;
            }

            std::this_thread::sleep_for(100ms);
        }

        LOG_PROFILE("=== Speed profile execution completed for port 0x%02X ===", port);
    }

    // Command queues for each port
    std::map<uint8_t, std::queue<std::function<void()>>> portCommandQueues;
    std::map<uint8_t, std::thread> portThreads;
    std::map<uint8_t, std::atomic<bool>> portThreadRunning;
    std::map<uint8_t, std::mutex> portQueueMutexes;
    std::map<uint8_t, std::condition_variable> portQueueCVs;
    std::map<uint8_t, double> motorCalibrationFactors;

    // Method to start the port handler thread
    void startPortThread(uint8_t port) {
        if (portThreadRunning[port]) return;

        portThreadRunning[port] = true;
        portThreads[port] = std::thread([this, port]() {
            this->portCommandProcessor(port);
        });
    }

    // Command processing method for the port
    void portCommandProcessor(uint8_t port) {
        while (portThreadRunning[port] && isValid) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(portQueueMutexes[port]);

                // We wait until a command appears or until a stop is requested
                portQueueCVs[port].wait(lock, [this, port]() {
                    return !portCommandQueues[port].empty() || !portThreadRunning[port];
                });

                if (!portThreadRunning[port]) break;

                if (portCommandQueues[port].empty()) continue;

                task = std::move(portCommandQueues[port].front());
                portCommandQueues[port].pop();
            }

            // We are completing the task
            try {
                task();
            }
            catch (const std::exception& ex) {
                LOG_ERROR("Error executing task for port 0x%02X: %s", port, ex.what());
            }
        }

        LOG_PROFILE("Command processor stopped for port 0x%02X", port);
    }

    // Method for adding a command to a port queue
    void enqueuePortCommand(uint8_t port, std::function<void()> task) {
        std::lock_guard<std::mutex> lock(portQueueMutexes[port]);

        // If the stream is not running yet, start it.
        if (!portThreadRunning.count(port) || !portThreadRunning[port]) {
            startPortThread(port);
        }

        portCommandQueues[port].push(std::move(task));
        portQueueCVs[port].notify_one();
    }

    bool startRelativeProfileController(uint8_t port, const std::vector<SpeedProfilePoint>& profilePoints, int timeoutMs) {
        initializeMotorState(port);
        auto& state = motorStates[port];

        // Complete stop of the previous profile
        state.profileActive = false;
        std::this_thread::sleep_for(100ms);

        // Complete reset of all positions under the mutex
        {
            std::lock_guard<std::mutex> lock(state.positionUpdateMutex);
            state.absolutePosition.store(0.0, std::memory_order_relaxed);
            state.segmentAccumulator.store(0.0, std::memory_order_relaxed);
            state.relativePosition.store(0.0, std::memory_order_relaxed);
            state.lastAbsolutePosition = 0.0;  // Reset the last absolute position
                      
            LOG_MOTOR("Reset motor position: Port=0x%02X, all positions set to 0.0", port);
        }

        // Install a new profile
        state.activeProfile = profilePoints;
        state.profileTimeoutMs = timeoutMs;
        state.currentSegmentIndex.store(0);
        state.profileActive.store(true);

        if (!profilePoints.empty()) {
            state.segmentTarget.store(profilePoints[0].distance);
        }

        LOG_PROFILE("Starting relative profile: Segments=%zu, Timeout=%dms",
            profilePoints.size(), timeoutMs);

        // Start the controller
        std::thread controllerThread([this, port]() {
            this->relativeProfileController(port);
            });
        controllerThread.detach();

        return true;
    }

    void initializeMotorState(uint8_t port) {
        if (!motorStates.count(port)) {
            // The correct way to initialize
            motorStates[port] = MotorState();
            LOG_DEBUG("Initialized motor state for port 0x%02X", port);
        }
    }

    void relativeProfileController(uint8_t port)
    {
        auto& state = motorStates[port];
        LOG_ENCODER("=== STARTING PRECISE PROFILE CONTROLLER ===");

        state.currentSegmentIndex.store(0, std::memory_order_relaxed);
        state.profileActive.store(true, std::memory_order_relaxed);

        startContinuousEncoderPolling(port);

        auto profileStartTime = std::chrono::steady_clock::now();
        auto lastControlTime = profileStartTime;
        bool profileCompleted = false;

        if (!state.activeProfile.empty()) {
            startSegment(port, 0);
        }

        while (!profileCompleted && state.profileActive && isValid) {
            auto currentTime = std::chrono::steady_clock::now();
            auto timeSinceLastControl = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastControlTime);

            lastControlTime = currentTime;

            int currentSegment = state.currentSegmentIndex.load();

            if (currentSegment >= state.activeProfile.size()) {
                setMotorSpeed(port, 0);
                LOG_PROFILE("PROFILE COMPLETED: All segments finished");
                profileCompleted = true;
                break;
            }

            const auto& segment = state.activeProfile[currentSegment];

            double traveled;
            {
                std::lock_guard<std::mutex> lock(state.positionUpdateMutex);
                traveled = state.segmentAccumulator.load(std::memory_order_relaxed);
            }

            static auto lastLogTime = profileStartTime;
            auto timeSinceLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastLogTime);
            if (timeSinceLastLog.count() > 500) {
                LOG_PROFILE("SEGMENT %d: Traveled=%.3f/%.3f rev (%.1f%%), Speed=%d",
                    currentSegment, traveled, segment.distance,
                    segment.distance > 0 ? (traveled / segment.distance) * 100 : 0,
                    segment.speed);
                lastLogTime = currentTime;
            }

            if (traveled >= segment.distance - segment.tolerance) {
                LOG_PROFILE("=== SEGMENT %d COMPLETED ===", currentSegment);
                LOG_PROFILE("Traveled %.3f of %.3f revolutions", traveled, segment.distance);

                int nextSegment = currentSegment + 1;
                state.currentSegmentIndex.store(nextSegment);

                if (nextSegment < state.activeProfile.size()) {

                    {
                        std::lock_guard<std::mutex> lock(state.positionUpdateMutex);
                        state.segmentAccumulator.store(0.0, std::memory_order_relaxed);
                    }
                    startSegment(port, nextSegment);
                }
                else {
                    setMotorSpeed(port, 0);
                    LOG_PROFILE("=== PROFILE COMPLETED ===");
                    profileCompleted = true;
                }
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - profileStartTime);
            if (elapsed.count() > state.profileTimeoutMs) {
                LOG_WARNING("PROFILE TIMEOUT: %d ms elapsed", elapsed.count());
                LOG_PROFILE("Current segment: %d, Traveled: %.3f/%.3f",
                    currentSegment, traveled, segment.distance);
                setMotorSpeed(port, 0);
                state.profileActive = false;
                break;
            }
        }

        state.profileActive = false;
        stopEncoderPolling(port);

        if (motorThreads.count(port) && motorThreads[port].joinable()) {
            motorThreads[port].join();
            motorThreads.erase(port);
        }

        LOG_PROFILE("Profile controller stopped for port 0x%02X", port);
    }

    void startSegment(uint8_t port, int segmentIndex) {
        auto& state = motorStates[port];
        const auto& segment = state.activeProfile[segmentIndex];

        {
            std::lock_guard<std::mutex> lock(state.positionUpdateMutex);
            // Reset ONLY the segment drive
            state.segmentAccumulator.store(0.0, std::memory_order_relaxed);
        }

        // Logging initial values ​​for debugging
        double initialAbs = state.absolutePosition.load(std::memory_order_relaxed);
        double initialSeg = state.segmentAccumulator.load(std::memory_order_relaxed);

        // Setting the motor speed
        setMotorSpeed(port, segment.speed);

        LOG_PROFILE(">>> STARTING SEGMENT %d: Target=%.3f rev, Speed=%d",
            segmentIndex, segment.distance, segment.speed);
        LOG_PROFILE(">>> INITIAL VALUES: AbsPos=%.3f, SegAcc=%.3f", initialAbs, initialSeg);
    }

    void startContinuousEncoderPolling(uint8_t port)
    {
        stopEncoderPolling(port);

        LOG_ENCODER("Starting optimized encoder polling for port 0x%02X", port);

        motorThreads[port] = std::thread([this, port]() {
            auto& state = motorStates[port];
            auto lastRequestTime = std::chrono::steady_clock::now();
            const std::chrono::milliseconds requestInterval(5);

            while (isValid && state.profileActive.load(std::memory_order_relaxed)) {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsed = currentTime - lastRequestTime;

                if (elapsed >= requestInterval) {
                    pollEncoderPosition(port);
                    lastRequestTime = currentTime;
                }
            }
            LOG_ENCODER("Encoder polling thread finished for port 0x%02X", port);
            });
    }

    void resetEncoderPosition(uint8_t port) {
        LOG_ENCODER("=== MANUAL POSITION RESET ===");

        auto& state = motorStates[port];

        {
            std::lock_guard<std::mutex> lock(state.positionUpdateMutex);
            double currentAbsolute = state.absolutePosition.load();

            // We reset ALL positions
            state.absolutePosition.store(0.0);
            state.segmentAccumulator.store(0.0);
            state.relativePosition.store(0.0); // if this variable is still in use
            state.lastAbsolutePosition = 0.0;

            LOG_ENCODER("Reset: Port=0x%02X, Was=%.3f, Now=0.000", port, currentAbsolute);
        }
    }

    void pollEncoderPosition(uint8_t port) {
        // Encoder position query command
        std::vector<uint8_t> requestCmd =
        {
            0x05,       // Length
            0x00,       // Hub ID
            0x21,       // Port Information Request
            port,       // Port
            0x00        // Mode: position
        };

        sendCommandVector(requestCmd);
    }

    // New try

public:

    // Updated method for testing
    bool testEncoderFunctionality(IPrinter* printer) {
        if (!printer) return false;

        LOG_DEBUG("FUNCTION do not do everything");
    }

private:

    void startEncoderPolling(uint8_t port)
    {
        // We stop the previous survey if there was one
        stopEncoderPolling(port);

        LOG_ENCODER("Encoder polling started for port 0x%02X", port);

        // Launching a new survey stream
        motorThreads[port] = std::thread([this, port]() {
            while (isValid && !stopRequested) { // Limit the number of requests
                pollEncoderPosition(port);
                std::this_thread::sleep_for(50ms); // Request every 50ms
            }
            LOG_ENCODER("Encoder polling stopped for port 0x%02X after %d requests", port);
            });
    }

    bool quickEncoderTest(uint8_t port)
    {
        LOG_ENCODER("=== QUICK ENCODER TEST (REAL-TIME) ===");

        // Resetting the position
        resetEncoderPosition(port);

        // We get the initial position
        double startPos = getMotorPosition(port);
        LOG_ENCODER("Start position: %.3f", startPos);

        // We start polling the encoder to activate updates
        startEncoderPolling(port);

        // We'll wait a bit to get some initial data.
        std::this_thread::sleep_for(100ms);

        // We receive a position after activating the survey
        startPos = getMotorPosition(port);
        LOG_ENCODER("Position after polling start: %.3f", startPos);

        // We start the engine for a short time
        setMotorSpeed(port, 40);
        LOG_MOTOR("Rotating motor at speed 40...");

        // We wait and measure the change in position
        auto startTime = std::chrono::steady_clock::now();
        double maxPosition = startPos;
        int measurements = 0;

        while (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count() < 300) {

            double currentPos = getMotorPosition(port);
            if (currentPos > maxPosition) {
                maxPosition = currentPos;
            }

            // Logging progress
            if (measurements % 5 == 0) { // Every 5 measurements
                LOG_PROFILE("Test loop %d: position=%.3f", measurements, currentPos);
            }

            measurements++;
            std::this_thread::sleep_for(50ms);
        }

        // We stop the engine
        setMotorSpeed(port, 0);
        LOG_MOTOR("Setting motor speed: Port=0x%02X, Speed=0", port);

        // Let the engine stop
        std::this_thread::sleep_for(100ms);

        // Final Dimension
        double finalPos = getMotorPosition(port);
        double positionChange = finalPos - startPos;

        LOG_PROFILE("Position after 300ms: %.3f (change: %.3f), measurements: %d",
            finalPos, positionChange, measurements);

        bool success = (positionChange > 0.05); // Minimum expected change
        LOG_ENCODER(success ? "SUCCESS: Encoder working! Position changed from %.3f to %.3f" :
            "FAILED: Encoder not responding to motor movement",
            startPos, finalPos);

        // Stopping the survey
        stopEncoderPolling(port);

        if (!success) {
            LOG_ENCODER("Final position: %.3f", finalPos);
            LOG_ENCODER("Last received encoder data analysis:");
            LOG_ENCODER("  - Check if 0x45 notifications are being received");
            LOG_ENCODER("  - Check if position bytes are changing in 0x45 messages");
        }

        return success;
    }

    void stopEncoderPolling(uint8_t port) {
        if (motorThreads.count(port)) {
            // Setting stop flags
            if (motorStates.count(port)) {
                motorStates[port].profileActive.store(false, std::memory_order_relaxed);
            }

            // Wait for the thread to complete with a timeout
            if (motorThreads[port].joinable()) {
                auto& thread = motorThreads[port];
                if (thread.get_id() != std::this_thread::get_id()) {
                    // We give the thread 500ms to complete
                    for (int i = 0; i < 50 && thread.joinable(); i++) {
                        std::this_thread::sleep_for(10ms);
                    }
                    if (thread.joinable()) {
                        thread.detach(); // Forced detachment as a last resort
                        LOG_WARNING("WARNING: Encoder polling thread for port 0x%02X had to be detached", port);
                    }
                }
            }

            motorThreads.erase(port);
            LOG_ENCODER("Encoder polling stopped for port 0x%02X", port);
        }
    }

    void activateEncoderMode(uint8_t port)
    {
        if (!isValid || !peripheral.is_connected()) return;

        LOG_ENCODER("Activating encoder mode for port 0x%02X", port);

        // Basic encoder activation command
        std::vector<uint8_t> setupCommand = {
            0x09,       // Length
            0x00,       // Hub ID  
            0x41,       // Port Configuration Command
            port,       // Motor port
            0x00,       // Mode: Position (absolute position)
            0x00,       // Data Format
            0x01,       // Unit: degrees
            0x00,       // Range min
            0x00        // Range max
        };

        sendCommandVector(setupCommand);

        // Command to enable notifications
        std::vector<uint8_t> subscribeCommand = {
            0x08,       // Length
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            port,       // Port  
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe flag
            0x00        // Padding
        };

        sendCommandVector(subscribeCommand);

        std::this_thread::sleep_for(200ms);
        LOG_ENCODER("Encoder mode activated for port 0x%02X", port);
    }

private:

    struct ProfileExecution {
        std::atomic<bool> active{ false };
        std::atomic<bool> completed{ false };
        std::unique_ptr<std::thread> executionThread;
        SpeedProfile profile;

        ProfileExecution() = default;

        ProfileExecution(const ProfileExecution&) = delete;
        ProfileExecution& operator=(const ProfileExecution&) = delete;

        ProfileExecution(ProfileExecution&& other) noexcept
            : active(other.active.load())
            , completed(other.completed.load())
            , executionThread(std::move(other.executionThread))
            , profile(std::move(other.profile)) {
        }

        ProfileExecution& operator=(ProfileExecution&& other) noexcept {
            if (this != &other) {
                active.store(other.active.load());
                completed.store(other.completed.load());
                executionThread = std::move(other.executionThread);
                profile = std::move(other.profile);
            }
            return *this;
        }

        ~ProfileExecution() {
            active = false;
            if (executionThread && executionThread->joinable()) {
                executionThread->join();
            }
        }
    };

    std::map<uint8_t, ProfileExecution> profileExecutions;
    std::mutex profileExecutionsMutex;

    void executeSingleProfileThread(uint8_t port) {
        auto& execution = profileExecutions[port];

        try {
            bool success = executeSpeedProfile(&execution.profile);

            if (success) {
                LOG_PROFILE("Profile completed successfully for port 0x%02X", port);
            }
            else {
                LOG_ERROR("Profile failed for port 0x%02X", port);
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR("Exception in profile execution for port 0x%02X: %s", port, ex.what());
        }

        delete[] execution.profile.points;
        execution.profile.points = nullptr;

        execution.active = false;
        execution.completed = true;
    }

    void waitForAllProfiles(int timeoutMs) {
        auto startTime = std::chrono::steady_clock::now();

        while (true) {
            bool allCompleted = true;

            {
                std::lock_guard<std::mutex> lock(profileExecutionsMutex);
                for (auto& [port, execution] : profileExecutions) {
                    if (execution.active && !execution.completed) {
                        allCompleted = false;
                        break;
                    }
                }
            }

            if (allCompleted) {
                LOG_PROFILE("All profiles completed");
                break;
            }

            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

            if (elapsed.count() > timeoutMs) {
                LOG_WARNING("Timeout waiting for profiles to complete");
                stopAllProfiles();
                break;
            }

            std::this_thread::sleep_for(5ms);
        }
    }

    void stopAllProfiles() {
        std::lock_guard<std::mutex> lock(profileExecutionsMutex);

        LOG_INFO("Stopping all profiles");

        std::vector<uint8_t> portsToStop;
        for (auto& [port, execution] : profileExecutions) {
            if (execution.active) {
                portsToStop.push_back(port);
            }
        }

        for (uint8_t port : portsToStop) {
            stopProfileExecution(port);
        }

        LOG_INFO("All profiles stopped");
    }

    void startProfileExecution(uint8_t port, const SpeedProfile& profile) {
        std::lock_guard<std::mutex> lock(profileExecutionsMutex);

        if (profileExecutions.count(port)) {
            stopProfileExecution(port);
        }

        ProfileExecution execution;
        execution.profile.port = profile.port;
        execution.profile.count = profile.count;
        execution.profile.timeoutMs = profile.timeoutMs;

        if (profile.points && profile.count > 0) {
            execution.profile.points = new SpeedProfilePoint[profile.count];
            std::copy(profile.points, profile.points + profile.count, execution.profile.points);
        }
        else {
            execution.profile.points = nullptr;
        }

        execution.active = true;
        execution.completed = false;

        execution.executionThread = std::make_unique<std::thread>([this, port]() {
            this->executeSingleProfileThread(port);
            });

        profileExecutions[port] = std::move(execution);
        LOG_PROFILE("Started profile execution for port 0x%02X", port);
    }

    void stopProfileExecution(uint8_t port) {
        std::lock_guard<std::mutex> lock(profileExecutionsMutex);

        if (profileExecutions.count(port)) {
            auto& execution = profileExecutions[port];
            execution.active = false;

            setMotorSpeed(port, 0);

            if (execution.executionThread && execution.executionThread->joinable()) {
                execution.executionThread->join();
            }

            if (execution.profile.points) {
                delete[] execution.profile.points;
                execution.profile.points = nullptr;
            }

            profileExecutions.erase(port);
            LOG_PROFILE("Stopped profile execution for port 0x%02X", port);
        }
    }

private:
    void handleSystemCommandReply(const std::vector<uint8_t>& data) {
        if (data.size() < 6) {
            LOG_ERROR("System command reply too short: %zu bytes", data.size());
            return;
        }

        const uint8_t subcommand = data[3];

        LOG_DEBUG("System command reply - Subcommand: 0x%02X, Size: %zu", subcommand, data.size());

        std::string packetHex;
        for (size_t i = 0; i < std::min(data.size(), (size_t)10); i++) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X ", data[i]);
            packetHex += hex;
        }
        LOG_DEBUG("Packet data: %s", packetHex.c_str());

        bool isBatteryResponse = false;

        // Option 1: 0x0B - GET_BATTERY_LEVEL (old protocol)
        if (subcommand == 0x0B) {
            isBatteryResponse = true;
            LOG_INFO("Detected battery response with subcommand 0x0B (GET_BATTERY_LEVEL)");
        }
        // Option 2: 0x1B - GET_BATTERY_LEVEL (new protocol)
        else if (subcommand == 0x1B) {
            isBatteryResponse = true;
            LOG_INFO("Detected battery response with subcommand 0x1B (GET_BATTERY_LEVEL_V2)");
        }
        // Option 3: 0x06 - Battery Voltage Property
        else if (subcommand == 0x06) {
            isBatteryResponse = true;
            LOG_INFO("Detected battery response with subcommand 0x06 (Battery Voltage)");
        }
        // Check other possible subcommands
        else if (subcommand == 0x62 || subcommand == 0x63 || subcommand == 0x64) {
            LOG_DEBUG("Checking if subcommand 0x%02X is battery-related", subcommand);
            // This might also be an answer about the battery
        }

        if (isBatteryResponse) {
            // Check the packet length
            if (data.size() >= 6) {
                // Trying to retrieve the battery level from different positions
                uint8_t batteryLevelFromData = 0;

                // Trying different formats:
                if (data.size() >= 5) {
                    batteryLevelFromData = data[4]; // Most probable position
                    LOG_DEBUG("Battery level from data[4]: %u", batteryLevelFromData);
                }

                if (data.size() >= 6) {
                    LOG_DEBUG("Battery level from data[5]: %u", data[5]);
                }

                // If the value seems correct (0-100)
                if (batteryLevelFromData <= 100) {
                    batteryLevel.store(batteryLevelFromData);
                    lastBatteryUpdate = std::chrono::steady_clock::now();

                    LOG_INFO("Battery level updated: %u%% (from subcommand 0x%02X)",
                        batteryLevelFromData, subcommand);
                }
                else if (batteryLevelFromData > 0) {
                    // This may be voltage in millivolts or another format
                    LOG_WARNING("Received battery value %u (not 0-100), checking format",
                        batteryLevelFromData);

                    // Let's try to convert if these are millivolts (for example, 7.4V = 7400mV)
                    if (batteryLevelFromData > 1000 && batteryLevelFromData < 10000) {
                        // Convert millivolts to percentages (very roughly)
                        // 6.0V (6000) = 0%, 8.4V (8400) = 100%
                        uint8_t percent = static_cast<uint8_t>(
                            ((batteryLevelFromData - 6000) * 100) / (8400 - 6000)
                            );

                        if (percent > 100) percent = 100;
                        if (percent < 0) percent = 0;

                        batteryLevel.store(percent);
                        lastBatteryUpdate = std::chrono::steady_clock::now();

                        LOG_INFO("Battery voltage: %umV -> %u%% (approximate)",
                            batteryLevelFromData, percent);
                    }
                }
            }
        }
        else {
            LOG_DEBUG("Unknown system command reply with subcommand 0x%02X (not battery)", subcommand);
        }
    }

public:
    bool requestBatteryLevel() {
        if (!isValid || !peripheral.is_connected()) {
            LOG_ERROR("Battery level request: printer not connected");
            return false;
        }

        std::lock_guard<std::mutex> lock(operationMutex);

        try {
            // Technic Hub automatically sends the battery when connected
            // But let's send a subscription request just in case
            std::vector<uint8_t> subscribeRequest = {
                0x05,   // Length
                0x00,   // Hub ID
                0x01,   // Hub Properties
                0x06,   // Battery Level Property
                0x02    // Operation: SUBSCRIBE
            };

            LOG_INFO("Subscribing to battery updates: 05 00 01 06 02");
            sendCommandVector(subscribeRequest);

            // We will also send a request to get the current value
            std::vector<uint8_t> getRequest = {
                0x05,   // Length
                0x00,   // Hub ID
                0x01,   // Hub Properties
                0x06,   // Battery Level Property
                0x01    // Operation: GET
            };

            LOG_INFO("Requesting battery level: 05 00 01 06 01");
            sendCommandVector(getRequest);

            lastBatteryUpdate = std::chrono::steady_clock::now();
            return true;
        }
        catch (const std::exception& ex) {
            LOG_ERROR("Error requesting battery level: %s", ex.what());
            return false;
        }
    }

    unsigned char getBatteryLevel() const {
        // Return the current value, but limit it to a maximum of 100
        uint8_t level = batteryLevel.load();
        return (level > 100) ? 100 : level;
    }

    bool isBatteryLevelFresh(int maxAgeSeconds = 30) const {
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - lastBatteryUpdate);
        return age.count() < maxAgeSeconds;
    }
};

// Main context and virtual table
namespace
{
    std::mutex contextsMutex;
    std::map<PrinterImplementation*, std::unique_ptr<PrinterImplementation>> contexts;

    // Virtual table functions - a bridge between C++ and C-INTERFACE
    bool printer_connect(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->connect();
    }

    bool printer_disconnect(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->disconnect();
    }

    bool printer_is_connected(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->isConnected();
    }

    void printer_destroy(IPrinter* self) {
        if (!self) return;

        try {
            PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);

            Implementation->safeShutdown();

            std::lock_guard<std::mutex> Lock(contextsMutex);

            if (contexts.find(Implementation) != contexts.end()) {
                contexts.erase(Implementation);
            }
        }
        catch (...) {
            // Ignore all errors
        }
    }

    void printer_set_motor_speed(IPrinter* self, unsigned char port, signed char speed) {
        if (!self || !self->vtable) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        Implementation->setMotorSpeed(port, speed);
    }

    void printer_rotate_motor(IPrinter* self, const MotorCommand* commands, int count) {
        if (!self || !self->vtable || !commands || count < -1) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        Implementation->rotateMotor(commands, count);
    }

    bool printer_printer_execute_speed_profile(IPrinter* self, const SpeedProfile* profile) {
        if (!self || !self->vtable || !profile) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->executeSpeedProfile(profile);
    }

    void printer_send_command(IPrinter* self, const unsigned char* command, int length) {
        if (!self || !self->vtable || !command || length < -1) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        Implementation->sendCommand(command, length);
    }

    bool printer_is_motor_moving(IPrinter* self, unsigned char port) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->isMotorMoving(port);
    }

    double printer_get_motor_position(IPrinter* self, unsigned char port) {
        if (!self || !self->vtable) return 0.0;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->getMotorPosition(port);
    }

    int printer_get_log_count(IPrinter* self) {
        if (!self || !self->vtable) return 0;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->getLogCount();
    }

    const char* printer_get_log_entry(IPrinter* self, int index) {
        if (!self || !self->vtable) return "";

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->getLogEntry(index);
    }

    void printer_printer_connection_info(IPrinter* self) {
        if (!self || !self->vtable) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->printConnectionInfo();
    }

    void printer_clear_log(IPrinter* self) {
        if (!self || !self->vtable) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->clearLog();
    }

    const char* printer_get_last_error(IPrinter* self) {
        if (!self || !self->vtable) return "";

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->getLastErrorMessage();
    }

    bool printer_test_encoder_functionality(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->testEncoderFunctionality(self);
    }

    bool printer_execute_speed_profiles(IPrinter* self, const SpeedProfile* profiles, int count) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->executeSpeedProfiles(profiles, count);
    }

    bool printer_request_battery_level(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->requestBatteryLevel();
    }

    unsigned char printer_get_battery_level(IPrinter* self) {
        if (!self || !self->vtable) return 0;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->getBatteryLevel();
    }

    bool printer_is_battery_fresh(IPrinter* self, int maxAgeSeconds) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->isBatteryLevelFresh(maxAgeSeconds);
    }

    void printer_set_log_categories(IPrinter* self, uint32_t categories) {
        if (!self || !self->vtable) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->setLogCategories(categories);
    }

    unsigned int printer_get_log_categories(IPrinter* self) {
        if (!self || !self->vtable) return 0;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->getLogCategories();
    }
}

// Virtual Method Table - C-INTERFACE
static IPrinterVirtualTable PrinterVTable = {
    printer_connect,
    printer_disconnect,
    printer_is_connected,
    printer_destroy,
    printer_rotate_motor,
    printer_set_motor_speed,
    printer_send_command,
    printer_printer_execute_speed_profile,
    printer_is_motor_moving,
    printer_get_motor_position,
    printer_get_log_count,
    printer_get_log_entry,
    printer_clear_log,
    printer_get_last_error,
    printer_printer_connection_info,
    printer_set_log_categories,
    printer_get_log_categories,
    printer_test_encoder_functionality,
    printer_execute_speed_profiles,
    printer_request_battery_level,
    printer_get_battery_level,
    printer_is_battery_fresh
};

// Tested function - remove after deep testing

bool testSpeedProfileAdvanced(IPrinter* Printer) {
    SpeedProfilePoint points[] = {
        {3.0, 20, 0.0005},
        {3.0, 30, 0.0005},
        {0.0, 0, 1.0}
    };

    SpeedProfile profile;
    profile.port = 0x00;
    profile.points = points;
    profile.count = 3;
    profile.timeoutMs = 30000;

    bool result = PrinterExecuteSpeedProfile(Printer, &profile);
    return result;
}

// C-INTERFACE functions are exported to DLL
extern "C"
{

    PRINTER_DRIVER_API IPrinter* CreatePrinter() {
        auto printer = std::make_unique<PrinterImplementation>();
        printer->interface.vtable = &PrinterVTable;

        PrinterImplementation* printerHandle = printer.get();
        std::lock_guard<std::mutex> Lock(contextsMutex);

        contexts[printerHandle] = std::move(printer);
        return &printerHandle->interface;
    }

    PRINTER_DRIVER_API void DestroyPrinter(IPrinter* printer)
    {
        if (!printer) return;

        try {
            if (printer->vtable && printer->vtable->printer_destroy) {
                printer->vtable->printer_destroy(printer);
            }
        }
        catch (...) {
            // Ignore all errors
        }
    }

    PRINTER_DRIVER_API bool PrinterConnect(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_connect) return false;

        return printer->vtable->printer_connect(printer);
    }

    PRINTER_DRIVER_API bool PrinterDisconnect(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_disconnect) return false;

        return printer->vtable->printer_disconnect(printer);
    }

    PRINTER_DRIVER_API bool IsConnected(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_connected) return false;

        return printer->vtable->printer_is_connected(printer);
    }

    PRINTER_DRIVER_API void PrinterRotateMotor(IPrinter* printer, MotorCommand* commands, int count) {
        if (!printer || !printer->vtable || !printer->vtable->printer_rotate_motor) return;

        return printer->vtable->printer_rotate_motor(printer, commands, count);
    }

    PRINTER_DRIVER_API void PrinterSendCommand(IPrinter* printer, const unsigned char* command, int length) {
        if (!printer || !printer->vtable || !printer->vtable->printer_send_command) return;

        return printer->vtable->printer_send_command(printer, command, length);
    }

    PRINTER_DRIVER_API void PrinterSetMotorSpeed(IPrinter* printer, unsigned char port, signed char speed) {
        if (!printer || !printer->vtable || !printer->vtable->printer_set_motor_speed) return;

        return printer->vtable->printer_set_motor_speed(printer, port, speed);
    }

    PRINTER_DRIVER_API int GetLogCount(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_log_count) return 0;

        return printer->vtable->printer_get_log_count(printer);
    }

    PRINTER_DRIVER_API const char* GetLogEntry(IPrinter* printer, int index) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_log_entry) return "";

        return printer->vtable->printer_get_log_entry(printer, index);
    }

    PRINTER_DRIVER_API void ClearLog(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_clear_log) return;

        return printer->vtable->printer_clear_log(printer);
    }

    PRINTER_DRIVER_API const char* GetLastErrorMessage(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_last_error) return "";

        return printer->vtable->printer_get_last_error(printer);
    }

    PRINTER_DRIVER_API bool PrinterIsMotorMoving(IPrinter* printer, int count) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_motor_moving) return false;

        return printer->vtable->printer_is_motor_moving(printer, count);
    }

    PRINTER_DRIVER_API double PrinterGetMotorPosition(IPrinter* printer, unsigned char port) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_motor_position) return 0.0;

        return printer->vtable->printer_get_motor_position(printer, port);
    }

    // Test function
    PRINTER_DRIVER_API bool RunPrinterTest(IPrinter* printer, const char* testName) {
        if (!printer || !testName) return false;

        std::string name(testName);

        if (name == "SpeedProfileAdvanced") {
            return testSpeedProfileAdvanced(printer);
        }
        else {
            return false;
        }
    }

    PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* printer) {
        if (printer) printer->vtable->printer_printer_connection_info(printer);
    }

    PRINTER_DRIVER_API void PrinterSetLogCategories(IPrinter* printer, unsigned int categories)
    {
        if (printer && categories > 0) printer->vtable->printer_set_log_categories(printer, categories);
    }

    PRINTER_DRIVER_API unsigned int PrinterGetLogCategories(IPrinter* printer)
    {
        if (!printer || !printer->vtable) return 0;

        return printer->vtable->printer_get_log_categories(printer);
    }

    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(IPrinter* printer, const SpeedProfile* profile) {
        if (!printer || !printer->vtable || !printer->vtable->printer_printer_execute_speed_profile) return false;

        return printer->vtable->printer_printer_execute_speed_profile(printer, profile);
    }

    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfiles(IPrinter* printer, const SpeedProfile* profiles, int count) {
        if (!printer || !printer->vtable) return false;

        if (printer->vtable->printer_execute_speed_profiles) {
            return printer->vtable->printer_execute_speed_profiles(printer, profiles, count);
        }
        else {
            return false;
        }
    }

    PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_request_battery_level) return false;

        return printer->vtable->printer_request_battery_level(printer);
    }

    PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_battery_level) return 0;

        return printer->vtable->printer_get_battery_level(printer);
    }

    PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(IPrinter* printer, int maxAgeSeconds) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_battery_fresh) return false;

        return printer->vtable->printer_is_battery_fresh(printer, maxAgeSeconds);
    }
}
