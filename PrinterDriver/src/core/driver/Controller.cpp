#include "Controller.h"
#include "protocol/PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <limits>

constexpr uint8_t REPLY_STATUS = 0x80;
constexpr uint8_t REPLY_PONG = 0x81;
constexpr uint8_t REPLY_ERROR = 0xFF;

Controller::Controller(ITransport& transportPointer)
	: transport(transportPointer) {
}

namespace {
	int64_t nowMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}
}

template<typename T>
bool Controller::sendCommand(uint8_t axis, uint8_t cmd, const T& payload) {
	constexpr size_t payload_size = sizeof(T);
	// create raw frame
	size_t frameSize = sizeof(FrameHeader) + payload_size + 1; // + crc data
	std::vector<uint8_t> rawFrame(frameSize);
	FrameHeader* hdr = reinterpret_cast<FrameHeader*>(rawFrame.data());
	hdr->sync = 0xAA;
	hdr->length = 2 + payload_size; // axis + cmd + payload
	hdr->axis = axis;
	hdr->cmd = cmd;
	memcpy(rawFrame.data() + sizeof(FrameHeader), &payload, payload_size);
	uint8_t crc = crc8(rawFrame.data(), frameSize - 1);
	rawFrame[frameSize - 1] = crc;
	return sendRawFrame(rawFrame.data(), frameSize);
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

				LOG_BLUETOOTH("char %s props: %d", characteristic.characteristicUuid.c_str(), characteristic.serviceUUid.c_str());

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

void Controller::onData(const uint8_t* data, size_t length) {
	if (length == 0) {
		return;
	}

	// Отмечаем «хаб жив» до фильтрации type=0x00 — нам важен сам факт RX
	lastHubRxMs.store(nowMs(), std::memory_order_release);

	uint8_t type = data[0];
	const uint8_t* payload = data + 1;
	size_t payloadLength = length - 1;

	// Игнорируем type=0x00, чтобы не блокировать RX-поток логгером
	if (type == 0x00) {
		return;
	}

	LOG_INFO("Runtime RX: type = 0x%02X, length = %zu", type, length);
	if (type == 0x01) { // stdout
		std::string text(reinterpret_cast<const char*>(payload), payloadLength);
		LOG_INFO("Hub stdout: %s", text.c_str());

		// --- Motor config telemetry ---
		if (text.find("LOG_PID ") != std::string::npos) {
			LOG_INFO("[CONFIG] PID %s", text.c_str() + 8);
			return;
		}
		if (text.find("LOG_LIMITS ") != std::string::npos) {
			LOG_INFO("[CONFIG] LIMITS %s", text.c_str() + 11);
			return;
		}
		if (text.find("LOG_TOL ") != std::string::npos) {
			LOG_INFO("[CONFIG] TOL %s", text.c_str() + 8);
			return;
		}
		if (text.find("LOG_CONFIG_END") != std::string::npos) {
			LOG_INFO("[CONFIG] end of motor config block");
			return;
		}

		// ============================================================
		// XY FLOW CONTROL
		// BUFFER_FULL — новый токен от хаба, работает наравне с FLOW_STOP.
		// Ставим lastFlowBlockMs, чтобы watchdog в TX-цикле знал,
		// с какого момента мы заблокированы.
		// ============================================================
		if (text.find("FLOW_STOP") != std::string::npos ||
			text.find("BUFFER_FULL") != std::string::npos ||
			text.find("COMMAND_PAUSE") != std::string::npos) {
			if (!remoteBufferFull.exchange(true, std::memory_order_acq_rel)) {
				lastFlowBlockMs.store(nowMs(), std::memory_order_release);
				LOG_WARNING("Host paused: hub XY buffer full");
			}
		}
		else if (text.find("FLOW_RESUME") != std::string::npos ||
			text.find("BUFFER_RESUME") != std::string::npos ||
			text.find("COMMAND_RESUME") != std::string::npos) {
			if (remoteBufferFull.exchange(false, std::memory_order_acq_rel)) {
				LOG_INFO("Host resumed: hub XY buffer has space");
			}
		}
		// --- Z backpressure ---
		else if (text.find("Z_STOP") != std::string::npos) {
			zRemoteFull.store(true, std::memory_order_release);
			LOG_WARNING("Z backpressure: robot Z queue high, buffering on host");
		}
		else if (text.find("Z_RESUME") != std::string::npos) {
			zRemoteFull.store(false, std::memory_order_release);
			LOG_INFO("Z backpressure: robot Z queue drained, resuming");
		}
		else if (text.find("Z_QUEUE_FULL") != std::string::npos) {
			zRemoteFull.store(true, std::memory_order_release);
			LOG_ERROR("Z_QUEUE_FULL from hub — forcing Z backpressure");
		}
		// --- Сброс при старте нового job ---
		else if (text.find("START_THRESHOLD_SET") != std::string::npos) {
			zRemoteFull.store(false, std::memory_order_release);
			remoteBufferFull.store(false, std::memory_order_release);
		}
	}
	else if (type == REPLY_STATUS) {
		// no-op
	}
	else if (type == REPLY_PONG) {
		LOG_INFO("Pong received");
	}
	else if (type == REPLY_ERROR) {
		if (payloadLength >= 3) {
			uint8_t axis = payload[0];
			uint8_t cmd = payload[1];
			uint8_t err = payload[2];
			LOG_WARNING("Hub error: axis = %d cmd = 0x%02X err = %d", axis, cmd, err);
		}
	}
}

bool Controller::sendStartThreshold(uint16_t threshold) {
	uint8_t packet[4];

	packet[0] = MOTION_TRANSPORT_PREFIX;
	packet[1] = CMD_SET_START_THRESHOLD;

	std::memcpy(&packet[2], &threshold, sizeof(threshold));

	return transport.write(pybricksCommandEvent, packet, sizeof(packet), true);
}

bool Controller::sendZCommand(int16_t angle, uint32_t afterXY) {
	uint8_t packet[8];

	packet[0] = MOTION_TRANSPORT_PREFIX;
	packet[1] = CMD_Z;

	std::memcpy(&packet[2], &angle, sizeof(angle));
	std::memcpy(&packet[4], &afterXY, sizeof(afterXY));

	return transport.write(pybricksCommandEvent, packet, sizeof(packet), true);
}

bool Controller::sendMotionProgram(const MotionProgram& program) {
	if (!connected || !transport.isConnected() ||
		pybricksCommandEvent.characteristicUuid.empty()) {
		LOG_ERROR("Motion program: not connected");
		return false;
	}
	if (program.startThreshold > std::numeric_limits<uint16_t>::max()) {
		LOG_ERROR("Motion program: start threshold too large");
		return false;
	}

	zRemoteFull.store(false, std::memory_order_release);
	remoteBufferFull.store(false, std::memory_order_release);
	lastHubRxMs.store(nowMs(), std::memory_order_release);
	lastFlowBlockMs.store(nowMs(), std::memory_order_release);

	if (!sendStartThreshold(static_cast<uint16_t>(program.startThreshold))) {
		LOG_ERROR("Failed to send start threshold");
		return false;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	const auto start = std::chrono::steady_clock::now();

	// ============================================================
	// БЕЗ ограничения размера pendingZ.
	// 98 Z × ~16 байт = 1.5 KB — память бесплатна.
	// Если hub 4 минуты печатает XY и его Z-очередь стоит на
	// HIGH_WATERMARK, host НЕ сдаётся: он копит Z и продолжает
	// слать XY. XY продвигает executed_xy, что разблокирует Z
	// на хабе → hub шлёт Z_RESUME → pendingZ сливается.
	// Deadlock невозможен по конструкции.
	// ============================================================
	std::deque<MotionCommand> pendingZ;

	size_t progIdx = 0;
	size_t sentPackets = 0;
	size_t sentZ = 0;
	size_t xyBlockedMs = 0;
	size_t zBlockedMs = 0;
	size_t maxPendingZ = 0;

	constexpr int64_t PROGRESS_LOG_INTERVAL_MS = 10000;
	constexpr int64_t HUB_SILENT_ABORT_MS = 120000;

	int64_t lastProgressLogMs = nowMs();

	// Сливает Z в hub пока zRemoteFull = false.
	auto flushZ = [&]() -> bool {
		while (!pendingZ.empty() && !zRemoteFull.load(std::memory_order_acquire)) {
			const auto& z = pendingZ.front();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			if (!sendZCommand(z.zAngle, z.afterXY)) {
				LOG_ERROR("Failed to send Z (angle=%d afterXY=%u)",
					z.zAngle, z.afterXY);
				return false;
			}
			pendingZ.pop_front();
			sentZ++;
		}
		if (pendingZ.size() > maxPendingZ) maxPendingZ = pendingZ.size();
		return true;
		};

	auto yieldForRx = []() {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		};

	while (progIdx < program.commands.size() || !pendingZ.empty()) {
		bool did_work = false;

		// (1) Пытаемся слить pendingZ, если hub не против
		if (!pendingZ.empty() && !zRemoteFull.load(std::memory_order_acquire)) {
			const size_t before = pendingZ.size();
			if (!flushZ()) return false;
			if (pendingZ.size() < before) did_work = true;
		}

		// (2) Обрабатываем следующую команду программы
		if (progIdx < program.commands.size()) {
			const auto& cmd = program.commands[progIdx];

			if (cmd.type == MotionCommandType::XY_PACKET) {
				// XY: шлём только когда нет backpressure
				if (!remoteBufferFull.load(std::memory_order_acquire)) {
					if (cmd.packetIndex >= program.packets.size()) {
						LOG_ERROR("Invalid XY packet index: %u", cmd.packetIndex);
						return false;
					}
					const auto& packet = program.packets[cmd.packetIndex];
					if (!transport.write(pybricksCommandEvent,
						packet.data.data(), packet.size, true)) {
						LOG_ERROR("Failed to send XY packet %u", cmd.packetIndex);
						return false;
					}
					sentPackets++;
					progIdx++;
					did_work = true;
					yieldForRx();
				}
			}
			else {
				// Z: кладём в pendingZ (без ограничения) и пробуем слить
				pendingZ.push_back(cmd);
				progIdx++;
				did_work = true;

				if (!pendingZ.empty() && !zRemoteFull.load(std::memory_order_acquire)) {
					if (!flushZ()) return false;
				}
				yieldForRx();
			}
		}

		// (3) Заблокированы: ждём, проверяем liveness
		if (!did_work) {
			if (!connected.load() || !transport.isConnected()) {
				LOG_ERROR("Motion TX aborted: connection lost "
					"(%zu cmds remaining, %zu Z pending)",
					program.commands.size() - progIdx, pendingZ.size());
				return false;
			}

			const int64_t idleMs = nowMs() - lastHubRxMs.load(std::memory_order_acquire);
			if (idleMs > HUB_SILENT_ABORT_MS) {
				LOG_ERROR("Motion TX aborted: hub silent %lld ms",
					(long long)idleMs);
				return false;
			}

			if (remoteBufferFull.load(std::memory_order_acquire)) xyBlockedMs += 2;
			if (zRemoteFull.load(std::memory_order_acquire))     zBlockedMs += 2;

			std::this_thread::sleep_for(std::chrono::milliseconds(2));

			const int64_t now = nowMs();
			if (now - lastProgressLogMs > PROGRESS_LOG_INTERVAL_MS) {
				LOG_INFO("Progress: prog=%zu/%zu pendingZ=%zu "
					"xyBlocked=%zu ms zBlocked=%zu ms hubIdle=%lld ms",
					progIdx, program.commands.size(), pendingZ.size(),
					xyBlockedMs, zBlockedMs, (long long)idleMs);
				lastProgressLogMs = now;
			}
		}
	}

	const auto end = std::chrono::steady_clock::now();
	const double seconds = std::chrono::duration<double>(end - start).count();

	LOG_INFO("Motion program sent: xyPackets=%zu zSent=%zu "
		"xyBlocked=%zu ms zBlocked=%zu ms zMaxPending=%zu "
		"threshold=%zu estimated=%.3f sec tx=%.3f sec",
		sentPackets, sentZ, xyBlockedMs, zBlockedMs, maxPendingZ,
		program.startThreshold, program.estimatedDurationSec, seconds);
	return true;
}

bool Controller::executeCode(const std::string& filename) {
	PrinterConfig config;

	// Кинематика
	config.stepsPerMmX = 5.0;
	config.stepsPerMmY = 5.0;

	// Подачи
	config.maxFeedrate = 12000.0;       // было 24000
	config.defaultFeedrate = 1000.0;

	// Z
	config.zUpAngle = 90;
	config.zDownAngle = 0;

	// BLE
	config.maxStartThreshold = 900;
	config.startThresholdSafetyMargin = 5;

	// Планировщик — согласован с реальными лимитами хаба (speed=2000, accel=6000 deg/s²)
	config.maxAccel = 1000.0;
	config.junctionDeviation = 1.5;
	config.maxVelocitySteps = 1200.0;
	config.junctionMinFactor = 0.0;

	GCodeCompiler compiler(config);
	MotionProgram program;

	if (!compiler.compile(filename, program)) {
		LOG_ERROR("Failed to compile g-code: %s", filename.c_str());
		return false;
	}

	LOG_INFO("MotionProgram compiled: segments=%zu packets=%zu "
		"commands=%zu threshold=%zu estimated=%.3f s",
		program.xySegmentCount, program.packets.size(),
		program.commands.size(), program.startThreshold,
		program.estimatedDurationSec);
	// Детальная диагностика последовательности
	// Очень полезно на первом запуске

	for (size_t i = 0; i < program.commands.size(); ++i) {
		const MotionCommand& command = program.commands[i];

		if (command.type == MotionCommandType::XY_PACKET) {
			const auto& packet = program.packets[command.packetIndex];

			LOG_INFO("CMD[%zu] XY packet = %u segments = %u bytes = %u",
				i, command.packetIndex, packet.segmentsCount, packet.size);
		}
		else {
			LOG_INFO("CMD[%zu] Z angle = %d afterXY = %u",
				i, command.zAngle, command.afterXY);
		}
	}

	return sendMotionProgram(program);
}
