#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "protocol/Constants.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

// ========== Структуры команд ==========
#pragma pack(push, 1)
struct FrameHeader {
	uint8_t sync = 0xAA;
	uint8_t length;      // длина данных: axis + cmd + payload_size
	uint8_t axis;
	uint8_t cmd;
};

struct MovePayload {
	uint16_t duration_ms;
	int16_t start_speed;
	int16_t cruise_speed;
	int16_t end_speed;
	uint16_t accel;
};

struct SetLimitsPayload {
	uint32_t max_speed;
	uint32_t max_accel;
};

struct StatusReply {
	uint8_t reply_code;   // 0x80
	uint8_t axis;
	int32_t position;
	int32_t speed;
	uint8_t flags;
	uint8_t buffer_free;
	uint8_t buffer_size;
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
	void sendCommand(uint8_t axis, uint8_t cmd, const T& payload) {
		constexpr size_t payload_size = sizeof(T);
		// Выделяем буфер: [0x06][FrameHeader][payload][CRC]
		uint8_t buffer[1 + sizeof(FrameHeader) + payload_size + 1];

		// Префикс WriteStdin
		buffer[0] = 0x06;

		// Заголовок кадра
		FrameHeader* hdr = reinterpret_cast<FrameHeader*>(buffer + 1);
		hdr->sync = 0xAA;
		hdr->length = 2 + payload_size;   // axis + cmd + payload
		hdr->axis = axis;
		hdr->cmd = cmd;

		// Payload
		memcpy(buffer + 1 + sizeof(FrameHeader), &payload, payload_size);

		// CRC считается от части после префикса (т.е. от sync до конца payload)
		uint8_t crc = crc8(buffer + 1, sizeof(FrameHeader) + payload_size);
		buffer[sizeof(buffer) - 1] = crc;

		// Отправляем весь буфер (включая префикс)
		transport.write(pybricksCommandEvent, buffer, sizeof(buffer), true);
	}
	void sendCommand(uint8_t axis, uint8_t cmd);

	uint8_t crc8(const uint8_t* data, size_t len);

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