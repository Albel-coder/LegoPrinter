#pragma once

#include "../core/driver/interfaces/ITransport.h"

#include <functional>
#include <string>
#include <atomic>

class RuntimeSession {
public:
	using RuntimeCallback = std::function<void(const uint8_t* data, size_t length)>;

	explicit RuntimeSession(ITransport& transportPointer);

	bool connect(const std::string& address);
	void disconnect();

	bool send(const uint8_t* data, size_t length, bool withResponse = false);
	void setCallback(RuntimeCallback callback);

	bool isConnected() const;
	std::string getConnectedAddress() const;

private:
	ITransport& transport;
	RuntimeCallback callback;

	Characteristic pybricksCommandEvent;
	Characteristic pybricksCapabilities;
	std::string connectedAddress;

	std::atomic<bool> connected{ false };
	std::atomic<bool> subscribed{ false };

	bool discover();
	void onData(const Characteristic& characteristic, const uint8_t* data, size_t length);
};