#include "RuntimeSession.h"
#include "protocol/PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>

constexpr uint8_t COMMAND_MOVE = 0x01;
constexpr uint8_t COMMAND_STOP = 0x02;

constexpr uint8_t COMMAND_STATUS = 0x04;
constexpr uint8_t COMMAND_RESET = 0x05;
constexpr uint8_t COMMAND_PING = 0x06;

RuntimeSession::RuntimeSession(ITransport& transportPointer)
	: transport(transportPointer) {}

bool RuntimeSession::discover() {
	if (!transport.isConnected()) {
		LOG_BLUETOOTH("Runtime discover: transport not connected");
		return false;
	}

	pybricksCommandEvent = {};
	pybricksCapabilities = {};
	nusTxChar = {};
	nusRxChar = {};

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
		// Nordic UART Service (NUS)
		else if (service == protocol::NUS_SERVICE_UUID) {
			for (const auto& characteristic : transport.getCharacteristics(service)) {
				if (characteristic.characteristicUuid == protocol::NUS_TX_CHAR_UUID) {
					nusTxChar = characteristic;   // read
				}
				else if (characteristic.characteristicUuid == protocol::NUS_RX_CHAR_UUID) {
					nusRxChar = characteristic;   // write (notifications)
				}
			}
		}
	}

	if (pybricksCommandEvent.characteristicUuid.empty()) {
		LOG_ERROR("Runtime discover: Pybricks command/event not found");
		return false;
	}

	if (nusTxChar.characteristicUuid.empty() || nusRxChar.characteristicUuid.empty()) {
		LOG_ERROR("Runtime discover: NUS TX/RX characteristics not found");
		return false;
	}

	LOG_BLUETOOTH("Runtime discover: Pybricks command/event found");
	return true;
}

bool RuntimeSession::connect(const std::string& address) {
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

	LOG_BLUETOOTH("RuntimeSession::connect: discover OK, subscribing to %s",
		pybricksCommandEvent.characteristicUuid.c_str());

	if (!transport.subscribe(pybricksCommandEvent, [this](const Characteristic& characteristic, const uint8_t* data, size_t length) {
		onData(characteristic, data, length);
		})) {
		LOG_ERROR("Runtime connect: subscribe failed");
		transport.disconnect();
		return false;
	}

	subscribed = true;
	connected = true;
	connectedAddress = address;

	LOG_INFO("Runtime connected");
	return true;
}

void RuntimeSession::disconnect() {
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

	try	{
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
	callback = nullptr;
}

bool RuntimeSession::send(const uint8_t* data, size_t length, bool withResponse) {
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

void RuntimeSession::setCallback(RuntimeCallback callback) {
	this->callback = std::move(callback);
}

bool RuntimeSession::isConnected() const {
	return connected && transport.isConnected();
}

bool RuntimeSession::rotateMotor(uint8_t port, int32_t speed, int32_t angle, bool hold) {
	LOG_INFO("Starting rotate motor command");
	std::vector<uint8_t> packet;

	packet.push_back(COMMAND_MOVE);
	packet.push_back(0x01); // commands count
	packet.push_back(port);

	for (int i = 0; i < 4; ++i) {
		packet.push_back((speed >> (8 * i)) & 0xFF);
	}
	for (int i = 0; i < 4; ++i) {
		packet.push_back((angle >> (8 * i)) & 0xFF);
	}
	packet.push_back(hold ? 0x01 : 0x00);

	LOG_INFO("Write rotate motor command");
	return transport.write(nusTxChar, packet.data(), packet.size(), false);
}

bool RuntimeSession::stopAllMotors() {
	uint8_t command = COMMAND_STOP;
	return transport.write(nusTxChar, &command, 1, false);
}

bool RuntimeSession::resetEncoders() {
	uint8_t command = COMMAND_RESET;
	return transport.write(nusTxChar, &command, 1, false);
}

bool RuntimeSession::ping() {
	uint8_t command = COMMAND_PING;
	return transport.write(nusTxChar, &command, 1, false);
}

void RuntimeSession::setStatusCallback(StatusCallback callback) {
	statusCallback = std::move(callback);
}

void RuntimeSession::onData(const Characteristic&, const uint8_t* data, size_t length) {
	LOG_COMMAND("Runtime RX: %zu bytes", length);
	
	if (length >= 3 && memcmp(data, "ack", 3) == 0) {
		LOG_INFO("Runtime ACK received");
	}
	else if (length >= 4 && memcmp(data, "pong", 4) == 0) {
		LOG_INFO("Runtime PONG received");
	}
	else if (length >= 5 && data[0] == 'E') {
		std::string error(data, data + length);
		LOG_ERROR("Runtime error: %s", error.c_str());
	}
	else if (length == 17 && data[0] == COMMAND_STATUS) {
		if (statusCallback) {
			int32_t pos0 = *reinterpret_cast<const int32_t*>(data + 1);
			int32_t pos1 = *reinterpret_cast<const int32_t*>(data + 5);
			int32_t speed0 = *reinterpret_cast<const int32_t*>(data + 9);
			int32_t speed1 = *reinterpret_cast<const int32_t*>(data + 13);
			statusCallback(pos0, pos1, speed0, speed1);
		}
	}
}
