#include "RuntimeSession.h"
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
uint8_t crc8(const uint8_t* data, size_t len) {
	uint8_t crc = 0;
	for (size_t i = 0; i < len; ++i) {
		crc ^= data[i];
		for (int j = 0; j < 8; ++j) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ 0x31;
			}
			else {
				crc <<= 1;
			}
		}
	}
	return crc;
}

constexpr uint8_t COMMAND_MOVE = 0x01;
constexpr uint8_t COMMAND_STOP = 0x02;

constexpr uint8_t COMMAND_STATUS = 0x04;
constexpr uint8_t COMMAND_RESET = 0x05;
constexpr uint8_t COMMAND_PING = 0x06;

template<typename T>
void RuntimeSession::sendCommand(uint8_t axis, uint8_t cmd, const T& payload) {
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

void RuntimeSession::sendCommand(uint8_t axis, uint8_t cmd) {
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

void RuntimeSession::drawArcContinuous(float radius, float start_angle, float end_angle, float feedrate) {
	const float steps_per_mm = 100.0f;   // перевод мм в градусы мотора
	const float dt = 0.020f;             // 20 мс между точками
	const int num_points = static_cast<int>((end_angle - start_angle) * radius / (feedrate * dt)) + 1;

	// Предварительно очищаем буферы (на всякий случай)
	sendCommand(0, CMD_CLEAR_BUFFER);
	sendCommand(1, CMD_CLEAR_BUFFER);

	// Текущие позиции (хост должен отслеживать)
	static float current_x = 0.0f, current_y = 0.0f;

	// Генерируем и отправляем точки
	for (int i = 0; i <= num_points; ++i) {
		float t = i / (float)num_points;
		float angle = start_angle + t * (end_angle - start_angle);
		float x = radius * cosf(angle);
		float y = radius * sinf(angle);

		// Переводим в градусы мотора
		int32_t target_x = static_cast<int32_t>(x * steps_per_mm);
		int32_t target_y = static_cast<int32_t>(y * steps_per_mm);

		// Отправляем обновление цели для обеих осей
		UpdateTargetPayload cmd_x = { target_x, static_cast<uint16_t>(feedrate), 0 };
		UpdateTargetPayload cmd_y = { target_y, static_cast<uint16_t>(feedrate), 0 };
		sendCommand(0, CMD_UPDATE_TARGET, cmd_x);
		sendCommand(1, CMD_UPDATE_TARGET, cmd_y);

		// Выдерживаем интервал отправки (не обязательно, но для стабильности)
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // ~20 Гц отправка
	}

	// После последней точки можно отправить STOP (HOLD) для фиксации
	StopPayload stop_hold = { 1 }; // HOLD
	sendCommand(0, CMD_STOP, stop_hold);
	sendCommand(1, CMD_STOP, stop_hold);
}

struct StatusReply {
	uint8_t reply_code;   // 0x80
	uint8_t axis;
	int32_t position;
	int32_t speed;
	uint8_t flags;
	uint8_t buffer_free;
};

void parseStatusReply(const uint8_t* data, size_t len) {
	if (len < 10) {
		return;
	}
	StatusReply reply;
	reply.reply_code = data[0];
	reply.axis = data[1];
	reply.position = *reinterpret_cast<const int32_t*>(data + 2);
	reply.speed = *reinterpret_cast<const int32_t*>(data + 6);
	reply.flags = data[10];
	reply.buffer_free = data[11];
}

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
		LOG_INFO("Program stdout: %s", message.c_str());
	}
}
