#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct DeviceInfo {
	std::string address;
	std::string name;
	int16_t rssi = 0;
	std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
	std::vector<std::string> serviceUuids;
};

struct Characteristic {
	std::string serviceUUid;
	std::string characteristicUuid;
};

using DataCallback = std::function<void(const Characteristic& characteristic, const uint8_t* data, size_t length)>;
using ConnectionCallback = std::function<void(bool connected, const std::string& address)>;

class ITransport {
public:
	virtual ~ITransport() = default;

	virtual bool startScan(int timeoutSeconds) = 0;

	virtual void stopScan() = 0;
	
	virtual std::vector<DeviceInfo> getScanResults() const = 0;

	virtual bool connect(const std::string& address) = 0;

	virtual bool disconnect() = 0;

	virtual bool isConnected() = 0;

	virtual std::string getConnectedAddress() const = 0;

	virtual std::vector<std::string> getServices() const = 0;

	virtual std::vector<Characteristic> getCharacteristics(const std::string& serviceUUid) const = 0;

	virtual bool read(const Characteristic& characteristic, std::vector<uint8_t>& out) = 0;

	virtual bool write(const Characteristic& characteristic, const uint8_t* data, size_t length, bool withResponse = true) = 0;

	virtual bool subscribe(const Characteristic& characteristic, DataCallback callback) = 0;

	virtual bool unsubscribe(const Characteristic& characteristic) = 0;

	virtual size_t getMaxWriteSize() const = 0;

	virtual void setConnectionCallback(ConnectionCallback callback) = 0;

	virtual const char* getName() const = 0;
};

using TransportPtr = std::unique_ptr<ITransport>;