#pragma once

#include "../core/driver/interfaces/ITransport.h"

#include <functional>
#include <string>

class RuntimeSession {
public:
	using RuntimeCallback = std::function<void(const uint8_t* data, size_t length)>;

	explicit RuntimeSession(ITransport& transport);

	bool connect(const std::string& address);
	void disconnect();

	bool send(const uint8_t* data, size_t length, bool withResponse = false);
	void setCallback(RuntimeCallback callback);

	bool isConnected() const;
	std::string getConnectedAddress() const;

private:
	ITransport& transport;
	RuntimeCallback callback;

	Characteristic nusRx;
	Characteristic nusTx;
	std::string connectedAddress;

	bool discover();
	void onData(const Characteristic& characteristic, const uint8_t* data, size_t length);
};