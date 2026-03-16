#include "RuntimeSession.h"
#include "protocol/PrinterProtocol.h"

#include <algorithm>

RuntimeSession::RuntimeSession(ITransport& transport)
	: transport(transport) {}

bool RuntimeSession::discover() {
	if (!transport.isConnected()) {
		return false;
	}

	nusRx = {};
	nusTx = {};

	for (const auto& service : transport.getServices()) {
		if (service == protocol::NUS_SERVICE_UUID) {
			
			for (const auto& characteristic : transport.getCharacteristics(service)) {
				if (characteristic.characteristicUuid == protocol::NUS_RX_UUID) {
					nusRx = characteristic;
				}
				else if (characteristic.characteristicUuid == protocol::NUS_TX_UUID) {
					nusTx = characteristic;
				}
			}
		}
	}

	return !nusRx.characteristicUuid.empty() && !nusTx.characteristicUuid.empty();
}

bool RuntimeSession::connect(const std::string& address) {
	if (transport.isConnected()) {
		transport.disconnect();
	}

	if (!transport.connect(address)) {
		return false;
	}

	if (!discover()) {
		transport.disconnect();
		return false;
	}

	transport.subscribe(nusTx, [this](const Characteristic& characteristic, const uint8_t* data, size_t length) {
		onData(characteristic, data, length);
	});

	connectedAddress = address;
	return true;
}

void RuntimeSession::disconnect() {
	if (transport.isConnected() && !nusTx.characteristicUuid.empty()) {
		transport.unsubscribe(nusTx);
	}
	if (transport.isConnected()) {
		transport.disconnect();
	}

	connectedAddress.clear();
	nusRx = {};
	nusTx = {};
	callback = nullptr;
}

bool RuntimeSession::send(const uint8_t* data, size_t length, bool withResponse) {
	if (!transport.isConnected() || nusRx.characteristicUuid.empty()) {
		return false;
	}

	const size_t maxChunk = transport.getMaxWriteSize();
	size_t offset = 0;

	while (offset < length) {
		const size_t chunk = std::min(maxChunk, length - offset);
		if (!transport.write(nusRx, data + offset, chunk, withResponse)) {
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
	return transport.isConnected();
}

void RuntimeSession::onData(const Characteristic&, const uint8_t* data, size_t length) {
	if (callback) {
		callback(data, length);
	}
}