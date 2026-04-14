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
	std::vector<uint8_t> packet;

	packet.push_back(0x06);
	packet.push_back(COMMAND_MOVE);
	packet.push_back(port);

	for (int i = 0; i < 4; ++i) {
		packet.push_back((speed >> (i * 8)) & 0xFF);
	}
	for (int i = 0; i < 4; ++i) {
		packet.push_back((angle >> (i * 8)) & 0xFF);
	}

	packet.push_back(hold ? 1 : 0);

	LOG_INFO("Sending motor command (size = %zu)", packet.size());
	return transport.write(pybricksCommandEvent, packet.data(), packet.size(), true);
}

bool RuntimeSession::stopAllMotors() {
	std::vector<uint8_t> packet = { COMMAND_STOP };
	return transport.write(pybricksCommandEvent, packet.data(), packet.size(), true);
}

bool RuntimeSession::resetEncoders() {
	std::vector<uint8_t> packet = { COMMAND_RESET };
	return transport.write(pybricksCommandEvent, packet.data(), packet.size(), true);
}

bool RuntimeSession::ping() {
	std::vector<uint8_t> packet = { COMMAND_PING };
	return transport.write(pybricksCommandEvent, packet.data(), packet.size(), true);
}

void RuntimeSession::setStatusCallback(StatusCallback callback) {
	statusCallback = std::move(callback);
}

void RuntimeSession::onData(const uint8_t* data, size_t length) {
	if (length == 0) {
		return;
	}

	uint8_t type = data[0];
	const uint8_t* payload = data + 1;
	size_t payloadLength = length - 1;

	LOG_INFO("Runtime RX: type = 0x%02X, length = %zu", type, length);
	
	if (type == 0x01) {
		std::string message(reinterpret_cast<const char*>(payload), payloadLength);

		while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
			message.pop_back();
		}
		LOG_INFO("Program stdout: %s", message.c_str());

		if (message == "ready") {
			LOG_INFO("find ready flag");
		}
		else if (message == "pong") {
			LOG_INFO("find pong flag");
		}
		else if (message == "ack") {
			LOG_INFO("find ack flag");
		}
		else if (message.rfind("ERROR", 0) == 0) {
			LOG_ERROR("Program error: %s", message.c_str());
		}
	}
	else if (type == 0x00) {
		if (payloadLength >= 1) {
			
		}
	}
}
