#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "protocol/Constants.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

struct FrameHeader {
	uint8_t sync = 0xAA;
	uint8_t length;      // data length: axis + cmd + payloadSize
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

class Controller {
public:
	using RuntimeCallback = std::function<void(const uint8_t* data, size_t length)>;

	explicit Controller(ITransport& transportPointer);

	bool connect(const std::string& address);
	void disconnect();

	bool send(const uint8_t* data, size_t length, bool withResponse = false);

	bool isConnected() const;
	std::string getConnectedAddress() const;

	//bool rotateMotor(uint8_t port, int32_t speed, int32_t angle, bool hold);
	//bool stopAllMotors();
	//bool resetEncoders();
	//bool ping();

	static const uint8_t ESC_BYTE = 0x10;
	static const uint8_t ESC_SUBST_03 = 0x13;
	static const uint8_t ESC_SUBST_10 = 0x10;

	std::vector<uint8_t> escapeData(const uint8_t* data, size_t len);
	
	template<typename T>
	void sendCommand(uint8_t axis, uint8_t cmd, const T& payload) {
		constexpr size_t payload_size = sizeof(T);
		// Frame buffer BEFORE screening
		uint8_t rawFrame[sizeof(FrameHeader) + payload_size + 1]; // +1 for CRC

		// header
		FrameHeader* hdr = reinterpret_cast<FrameHeader*>(rawFrame);
		hdr->sync = 0xAA;
		hdr->length = 2 + payload_size; // axis + cmd + payload
		hdr->axis = axis;
		hdr->cmd = cmd;

		// Payload
		memcpy(rawFrame + sizeof(FrameHeader), &payload, payload_size);

		// CRC by unshielded data (sync ... payload)
		uint8_t crc = crc8(rawFrame, sizeof(FrameHeader) + payload_size);
		rawFrame[sizeof(FrameHeader) + payload_size] = crc;

		// Screen the entire frame (sync ... crc)
		std::vector<uint8_t> escaped = escapeData(rawFrame, sizeof(rawFrame));

		// We form the final buffer with the prefix 0x06
		std::vector<uint8_t> buffer;
		buffer.push_back(0x06);
		buffer.insert(buffer.end(), escaped.begin(), escaped.end());
		transport.write(pybricksCommandEvent, buffer.data(), buffer.size(), true);
	}

	void sendCommand(uint8_t axis, uint8_t cmd);

	uint8_t crc8(const uint8_t* data, size_t len);

	//void drawArcContinuous(float radius, float start_angle, float end_angle, float feedrate);

private:
	ITransport& transport;

	Characteristic pybricksCommandEvent;
	Characteristic pybricksCapabilities;
	std::string connectedAddress;

	std::atomic<bool> connected{ false };
	std::atomic<bool> subscribed{ false };

	bool discover();
	void onData(const uint8_t* data, size_t length);
};
