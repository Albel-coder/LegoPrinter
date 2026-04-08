#include "RuntimeSession.h"
#include "protocol/PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>

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
		if (service != protocol::PYBRICKS_SERVICE_UUID) {
			continue;
		}

		for (const auto& characteristic : transport.getCharacteristics(service)) {
			if (characteristic.characteristicUuid == protocol::PYBRICKS_COMMAND_EVENT_UUID) {
				pybricksCommandEvent = characteristic;
			}
			else if (characteristic.characteristicUuid == protocol::PYBRICKS_HUB_CAPABILITIES_UUID) {
				pybricksCapabilities = characteristic; // read-only, don't write in it
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

void RuntimeSession::onData(const Characteristic&, const uint8_t* data, size_t length) {
	LOG_COMMAND("Runtime RX: %zu bytes", length);
	if (callback) {
		callback(data, length);
	}
}
