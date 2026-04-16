#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "protocol/Constants.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#pragma pack(push, 1)
struct FrameHeader {
	uint8_t sync = 0xAA;
	uint8_t length;
	uint8_t axis;
	uint8_t cmd;
	// uint8_t payload[];
	// uint8_t crc;
};

// CMD_UPDATE_TARGET (0x10)
struct UpdateTargetPayload {
	int32_t target;
	uint16_t speed;   // новое поле
	uint16_t time_ms; // новое поле
};

// CMD_SET_LIMITS (0x20)
struct SetLimitsPayload {
	int32_t max_speed;
	int32_t max_accel;
};

// CMD_MOVE_VEL (0x11)
struct MoveVelPayload {
	int32_t speed;
};

// CMD_STOP (0x12)
struct StopPayload {
	uint8_t stop_type;   // 0 = COAST, 1 = HOLD
};

// CMD_ENABLE_WATCHDOG (0x50)
struct WatchdogPayload {
	uint16_t timeout_ms;
};
#pragma pack(pop)

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

	void drawArcContinuous(float radius, float start_angle, float end_angle, float feedrate);

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