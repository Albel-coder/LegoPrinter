#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "../logging/LogManager.h"
#include <simpleble/SimpleBLE.h>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

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
    bool startScan(int timeoutSeconds) override;
    void stopScan() override;
    std::vector<DeviceInfo> getScanResults() const override;

    bool connect(const std::string& address) override;
    bool disconnect() override;
    bool isConnected() override;
    std::string getConnectedAddress() const override;

    std::vector<std::string> getServices() const override;
    std::vector<Characteristic> getCharacteristics(const std::string& serviceUUid) const override;

    bool read(const Characteristic& characteristic, std::vector<uint8_t>& out) override;
    bool write(const Characteristic& characteristic, const uint8_t* data, size_t length, bool withResponse = true) override;

    bool subscribe(const Characteristic& characteristic, DataCallback callback) override;
    bool unsubscribe(const Characteristic& characteristic) override;

    size_t getMaxWriteSize() const override;

    void setConnectionCallback(ConnectionCallback callback) override;
    const char* getName() const override { return "SimpleBLE"; }

private:
    struct CharKey {
        std::string serviceUuid;
        std::string characteristicUuid;

        bool operator<(const CharKey& other) const {
            return std::tie(serviceUuid, characteristicUuid) <
                   std::tie(other.serviceUuid, other.characteristicUuid);
        }
    };

    void scanWorker(int timeoutSeconds);
    bool findPeripheralByAddress(const std::string& address, SimpleBLE::Peripheral& out);
    void cacheServicesAndCharacteristics();
    void clearCache();
    void dispatchNotification(const Characteristic& characteristic, const std::vector<uint8_t>& data);

private:
    mutable std::mutex stateMutex;
    std::atomic<bool> scanning{ false };
    std::atomic<bool> stopScanRequested{ false };
    std::thread scanThread;

    std::vector<DeviceInfo> scanResults;

    SimpleBLE::Adapter adapter;
    SimpleBLE::Peripheral peripheral;
    std::string connectedAddress;

    ConnectionCallback connectionCallback;
    std::map<std::string, std::vector<Characteristic>> cachedCharacteristics;
    std::vector<std::string> cachedServices;
    std::map<CharKey, DataCallback> subscriptions;

    size_t maxWriteSize = 20;
};
