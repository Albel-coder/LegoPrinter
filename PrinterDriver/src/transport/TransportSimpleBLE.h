#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "../logging/LogManager.h"
#include <simpleble/SimpleBLE.h>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>

class TransportSimpleBLE : public ITransport {
public:
    TransportSimpleBLE();
    ~TransportSimpleBLE() override;

    // Disable copying and moving
    TransportSimpleBLE(const TransportSimpleBLE&) = delete;
    TransportSimpleBLE& operator=(const TransportSimpleBLE&) = delete;
    TransportSimpleBLE(TransportSimpleBLE&&) = delete;
    TransportSimpleBLE& operator=(TransportSimpleBLE&&) = delete;

    // ITransport implementation
    bool open() override;
    bool close() override;
    bool write(const uint8_t* data, size_t length) override;
    bool isConnected() override;

    void setDataCallback(std::function<void(const uint8_t*, size_t)> callback) override;
    void setConnectionCallback(std::function<void(bool)> callback) override;
    const char* getName() const override { return "SimpleBLE"; }

private:

    //void scanAndConnect();
    void onNotification(const std::vector<uint8_t>& data);

    // BLE
    SimpleBLE::Adapter adapter_;
    SimpleBLE::Peripheral peripheral_;

    // Callbacks
    std::function<void(const uint8_t*, size_t)> dataCallback_;
    std::function<void(bool)> connectionCallback_;

    // Synchronization
    mutable std::mutex stateMutex_;
    std::condition_variable stateCV_;

    // Worker thread
    std::thread workerThread_;
    std::atomic<bool> stopRequested_{ false };
    std::atomic<bool> threadRunning_{ false };

    // Constants
    static const std::string LEGO_HUB_SERVICE_UUID;
    static const std::string LEGO_HUB_CHARACTERISTIC_UUID;

    std::atomic<bool> deviceFound_;
    std::mutex candidateMutex_;
    std::optional<SimpleBLE::Peripheral> peripheralCandidate_;

    // Helper methods
    void cleanup();
    void workerFunction();
};
