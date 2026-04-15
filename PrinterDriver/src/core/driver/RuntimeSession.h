#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "protocol/Constants.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

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

	bool rotateMotor(uint8_t port, int32_t speed, int32_t angle, bool hold);
	bool stopAllMotors();
	bool resetEncoders();
	bool ping();

	using StatusCallback = std::function<void(int32_t position0, int32_t position1, int32_t speed0, int32_t speed1)>;
	void setStatusCallback(StatusCallback callback);

	template<typename T>
	void sendCommand(uint8_t axis, uint8_t cmd, const T& payload);
	void sendCommand(uint8_t axis, uint8_t cmd);

private:
	ITransport& transport;
	RuntimeCallback callback;
	StatusCallback statusCallback;

	Characteristic pybricksCommandEvent;
	Characteristic pybricksCapabilities;
	std::string connectedAddress;

	std::atomic<bool> connected{ false };
	std::atomic<bool> subscribed{ false };

	bool discover();
	void onData(const uint8_t* data, size_t length);
};