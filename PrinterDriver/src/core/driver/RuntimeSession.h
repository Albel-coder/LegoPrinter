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

	static const uint8_t ESC_BYTE = 0x10;
	static const uint8_t ESC_SUBST_03 = 0x13;
	static const uint8_t ESC_SUBST_10 = 0x10;

	std::vector<uint8_t> escapeData(const uint8_t* data, size_t len) {
		std::vector<uint8_t> out;
		out.reserve(len * 2); // запас на случай экранирования
		for (size_t i = 0; i < len; ++i) {
			if (data[i] == 0x03) {
				out.push_back(ESC_BYTE);
				out.push_back(ESC_SUBST_03);
			}
			else if (data[i] == ESC_BYTE) {
				out.push_back(ESC_BYTE);
				out.push_back(ESC_SUBST_10);
			}
			else {
				out.push_back(data[i]);
			}
		}
		return out;
	}

	template<typename T>
	void sendCommand(uint8_t axis, uint8_t cmd, const T& payload) {
		constexpr size_t payload_size = sizeof(T);
		// Буфер для кадра ДО экранирования
		uint8_t rawFrame[sizeof(FrameHeader) + payload_size + 1]; // +1 для CRC

		// Заголовок
		FrameHeader* hdr = reinterpret_cast<FrameHeader*>(rawFrame);
		hdr->sync = 0xAA;
		hdr->length = 2 + payload_size;   // axis + cmd + payload
		hdr->axis = axis;
		hdr->cmd = cmd;

		// Payload
		memcpy(rawFrame + sizeof(FrameHeader), &payload, payload_size);

		// CRC по неэкранированным данным (sync ... payload)
		uint8_t crc = crc8(rawFrame, sizeof(FrameHeader) + payload_size);
		rawFrame[sizeof(FrameHeader) + payload_size] = crc;

		// Экранируем весь кадр (sync ... crc)
		std::vector<uint8_t> escaped = escapeData(rawFrame, sizeof(rawFrame));

		// Формируем финальный буфер с префиксом 0x06
		std::vector<uint8_t> buffer;
		buffer.push_back(0x06);
		buffer.insert(buffer.end(), escaped.begin(), escaped.end());

		// Отправка
		transport.write(pybricksCommandEvent, buffer.data(), buffer.size(), true);
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