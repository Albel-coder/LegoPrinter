#include "Controller.h"

#include "protocol/PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <cmath>

const uint8_t CMD_UPDATE_TARGET = 0x10;
const uint8_t CMD_MOVE_VEL = 0x11;
const uint8_t CMD_STOP = 0x12;
const uint8_t CMD_SET_LIMITS = 0x20;
const uint8_t CMD_RESET_POS = 0x21;
const uint8_t CMD_GET_STATUS = 0x30;
const uint8_t CMD_EMERGENCY_STOP = 0x40;
const uint8_t CMD_PING = 0x41;
const uint8_t CMD_CLEAR_BUFFER = 0x42;
const uint8_t CMD_ENABLE_WATCHDOG = 0x50;

// CRC8 Dallas/Maxim
static const uint8_t CRC8_TABLE[256] = {
	0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97,
	0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
	0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4,
	0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
	0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11,
	0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
	0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52,
	0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
	0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA,
	0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
	0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9,
	0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
	0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C,
	0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
	0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F,
	0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
	0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED,
	0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
	0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE,
	0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
	0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B,
	0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
	0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28,
	0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
	0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0,
	0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
	0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93,
	0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
	0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56,
	0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
	0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15,
	0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC
};

uint8_t Controller::crc8(const uint8_t* data, size_t len) {
	uint8_t crc = 0;
	for (size_t i = 0; i < len; ++i) {
		crc = CRC8_TABLE[crc ^ data[i]];
	}
	return crc;
}

Controller::Controller(ITransport& transportPointer)
	: transport(transportPointer) {
}

constexpr uint8_t COMMAND_MOVE = 0x01;
constexpr uint8_t COMMAND_STOP = 0x02;

constexpr uint8_t COMMAND_STATUS = 0x04;
constexpr uint8_t COMMAND_RESET = 0x05;
constexpr uint8_t COMMAND_PING = 0x06;

void Controller::sendCommand(uint8_t axis, uint8_t cmd) {
	uint8_t buffer[1 + sizeof(FrameHeader) + 1];
	buffer[0] = 0x06;
	FrameHeader* hdr = reinterpret_cast<FrameHeader*>(buffer + 1);
	hdr->sync = 0xAA;
	hdr->length = 2;   // only axis and cmd
	hdr->axis = axis;
	hdr->cmd = cmd;
	uint8_t crc = crc8(buffer + 1, sizeof(FrameHeader));
	buffer[sizeof(buffer) - 1] = crc;

	std::string hex;
	for (size_t i = 0; i < sizeof(buffer); ++i) {
		char buf[4];
		snprintf(buf, sizeof(buf), "%02X ", buffer[i]);
		hex += buf;
	}
	LOG_INFO("Sending packet: %s", hex.c_str());

	transport.write(pybricksCommandEvent, buffer, sizeof(buffer), true);
}

bool Controller::discover() {
	if (!transport.isConnected()) {
		LOG_BLUETOOTH("Runtime discover: transport not connected");
		return false;
	}

	pybricksCommandEvent = {};
	pybricksCapabilities = {};

	LOG_BLUETOOTH("Runtime discover: checking Pybricks service");

	for (const auto& service : transport.getServices()) {
		// Pybricks Command/Event service
		if (service == protocol::PYBRICKS_SERVICE_UUID) {
			for (const auto& characteristic : transport.getCharacteristics(service)) {
				if (characteristic.characteristicUuid == protocol::PYBRICKS_COMMAND_EVENT_UUID) {
					pybricksCommandEvent = characteristic;
				}
				else if (characteristic.characteristicUuid == protocol::PYBRICKS_HUB_CAPABILITIES_UUID) {
					pybricksCapabilities = characteristic; // read-only, don't write in it
				}
			}
		}
	}

	if (pybricksCommandEvent.characteristicUuid.empty()) {
		LOG_ERROR("Runtime discover: Pybricks command/event not found");
		return false;
	}

	LOG_BLUETOOTH("Runtime discover: Pybricks command/event found");
	return true;
}

bool Controller::connect(const std::string& address) {
	LOG_BLUETOOTH("RuntimeSession::connect: transport isConnected=%d, address=%s",
		transport.isConnected(), transport.getConnectedAddress().c_str());

	if (connected) {
		disconnect();
	}

	if (transport.isConnected() && transport.getConnectedAddress() != address) {
		transport.disconnect();
	}

	if (!transport.isConnected()) {
		if (!transport.connect(address)) {
			LOG_ERROR("Runtime connect failed");
			return false;
		}
	}

	if (!discover()) {
		LOG_ERROR("RuntimeSession::connect: discover failed");
		transport.disconnect();
		return false;
	}

	LOG_BLUETOOTH("RuntimeSession::connect: discover OK, subscribing to Pybricks Command/Event %s",
		pybricksCommandEvent.characteristicUuid.c_str());

	bool subscribed = transport.subscribe(pybricksCommandEvent, [this](const Characteristic&, const uint8_t* data, size_t length) {
		this->onData(data, length);
		});

	if (!subscribed) {
		LOG_ERROR("Failed to subscribe to Pybricks Command/Event");
		return false;
	}

	subscribed = true;
	connected = true;
	connectedAddress = address;

	LOG_INFO("Runtime connected");
	return true;
}

void Controller::disconnect() {
	LOG_BLUETOOTH("Runtime disconnect");

	const bool wasSubscribed = subscribed.exchange(false);
	connected.store(false);

	try {
		if (wasSubscribed && transport.isConnected() && !pybricksCommandEvent.characteristicUuid.empty()) {
			transport.unsubscribe(pybricksCommandEvent);
		}
	}
	catch (...) {
		LOG_WARNING("Runtime disconnect: unsubscribe failed");
	}

	try {
		if (transport.isConnected()) {
			transport.disconnect();
		}
	}
	catch (...) {
		LOG_WARNING("Runtime disconnect: transport disconnect failed");
	}

	connectedAddress.clear();
	pybricksCapabilities = {};
	pybricksCommandEvent = {};
}

bool Controller::send(const uint8_t* data, size_t length, bool withResponse) {
	if (!connected && !transport.isConnected() || pybricksCommandEvent.characteristicUuid.empty()) {
		LOG_ERROR("Runtime send: not connected");
		return false;
	}

	const size_t maxChunk = transport.getMaxWriteSize();
	size_t offset = 0;

	LOG_COMMAND("Runtime send: %zu bytes (chunk=%zu)", length, maxChunk);

	while (offset < length) {
		const size_t chunk = std::min(maxChunk, length - offset);
		if (!transport.write(pybricksCommandEvent, data + offset, chunk, withResponse)) {
			LOG_ERROR("Runtime send: write failed at offset %zu", offset);
			return false;
		}

		offset += chunk;
	}

	return true;
}

std::vector<uint8_t> Controller::escapeData(const uint8_t* data, size_t len) {
	std::vector<uint8_t> out;
	out.reserve(len * 2);
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

void Controller::onData(const uint8_t* data, size_t length) {
	if (length == 0) {
		return;
	}

	uint8_t type = data[0];
	const uint8_t* payload = data + 1;
	size_t payloadLength = length - 1;

	LOG_INFO("Runtime RX: type = 0x%02X, length = %zu", type, length);
	if (type == 0x01) {
		std::string hex;
		for (size_t i = 0; i < payloadLength; ++i) {
			char buf[4];
			snprintf(buf, sizeof(buf), "%02X ", payload[i]);
			hex += buf;
		}
		LOG_INFO("Program stdout HEX: %s", hex.c_str());

		std::string text(reinterpret_cast<const char*>(payload), payloadLength);
		LOG_INFO("Program stdout TEXT: %s", text.c_str());
	}
}
