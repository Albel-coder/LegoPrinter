#include "RuntimeSession.h"
#include "protocol/PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>

RuntimeSession::RuntimeSession(ITransport& transport)
	: transport(transport) {}

// Printer service
bool RuntimeSession::discover() {
	if (!transport.isConnected()) {
		LOG_BLUETOOTH("Runtime discover: transport not connected");
		return false;
	}

	printerCommandEvent = {};
	printerCapabilities = {};

	LOG_BLUETOOTH("Runtime discover: checking printer service");

	for (const auto& service : transport.getServices()) {
		if (service == protocol::PRINTER_SERVICE_UUID) {
			
			for (const auto& characteristic : transport.getCharacteristics(service)) {
				if (characteristic.characteristicUuid == protocol::PRINTER_COMMAND_EVENT_UUID) {
					printerCommandEvent = characteristic;
				}
				else if (characteristic.characteristicUuid == protocol::PRINTER_CAPABILITIES_UUID) {
					printerCapabilities = characteristic;
				}
			}
		}
	}

	if (printerCommandEvent.characteristicUuid.empty() || printerCapabilities.characteristicUuid.empty()) {
		LOG_ERROR("Runtime discover: printer command/event not found");
		return false;
	}

	LOG_BLUETOOTH("Runtime discover: printer command/event found");
	return true;
}

bool RuntimeSession::connect(const std::string& address) {
	LOG_BLUETOOTH("Runtime connect: %s", address.c_str());

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
		LOG_ERROR("Runtime connect: discover failed");
		transport.disconnect();
		return false;
	}

	if (!transport.subscribe(printerCommandEvent, [this](const Characteristic& characteristic, const uint8_t* data, size_t length) {
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

	if (!connected && !subscribed && !transport.isConnected()) {
		LOG_BLUETOOTH("Transport already disconnected, clearing state");
		connectedAddress.clear();
		printerCapabilities = {};
		printerCommandEvent = {};
		callback = nullptr;
		return;
	}

	LOG_BLUETOOTH("Runtime disconnect");

	// Flip flags first so a re-entrant call won`t do work twice
	const bool wasSubscribed = subscribed.exchange(false);
	connected.store(false);

	try {
		if (wasSubscribed && transport.isConnected() && !printerCommandEvent.characteristicUuid.empty()) {
			transport.unsubscribe(printerCommandEvent);
		}
	}
	catch (...)	{
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
	printerCapabilities = {};
	printerCommandEvent = {};
	callback = nullptr;
}

bool RuntimeSession::send(const uint8_t* data, size_t length, bool withResponse) {
	if (!connected && !transport.isConnected() || printerCapabilities.characteristicUuid.empty()) {
		LOG_ERROR("Runtime send: not connected");
		return false;
	}

	const size_t maxChunk = transport.getMaxWriteSize();
	size_t offset = 0;

	LOG_COMMAND("Runtime send: %zu bytes (chunk=%zu)", length, maxChunk);

	while (offset < length) {
		const size_t chunk = std::min(maxChunk, length - offset);
		if (!transport.write(printerCapabilities, data + offset, chunk, withResponse)) {
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

void RuntimeSession::onData(const Characteristic&, const uint8_t* data, size_t length) {
	LOG_COMMAND("Runtime RX: %zu bytes", length);
	if (callback) {
		callback(data, length);
	}
}