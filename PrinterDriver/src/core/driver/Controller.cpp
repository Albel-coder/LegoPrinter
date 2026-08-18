#include "Controller.h"
#include "protocol/PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

constexpr uint8_t REPLY_STATUS = 0x80;
constexpr uint8_t REPLY_PONG = 0x81;
constexpr uint8_t REPLY_ERROR = 0xFF;

constexpr uint8_t CMD_MOTION_BLOCK = 0x02;

Controller::Controller(ITransport& transportPointer)
	: transport(transportPointer) {
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

bool Controller::sendAngles(int32_t angleX, int32_t angleY) {
	if (!connected) {
		return false;
	}

	uint8_t packet[9];
	packet[0] = 0xAA;
	memcpy(&packet[1], &angleX, sizeof(angleX));
	memcpy(&packet[5], &angleY, sizeof(angleY));

	uint8_t buffer[10];
	buffer[0] = 0x06;
	memcpy(&buffer[1], packet, 9);

	return transport.write(pybricksCommandEvent, buffer, sizeof(buffer), true);
}

void Controller::onData(const uint8_t* data, size_t length) {
	if (length == 0) {
		return;
	}

	uint8_t type = data[0];
	const uint8_t* payload = data + 1;
	size_t payloadLength = length - 1;

	LOG_INFO("Runtime RX: type = 0x%02X, length = %zu", type, length);
	if (type == 0x01) { // stdout
		std::string text(reinterpret_cast<const char*>(payload), payloadLength);

		// Перехватываем управление потоком
		if (text.find("COMMAND_PAUSE") != std::string::npos) {
			remoteBufferFull.store(true);
			LOG_WARNING("Host paused: Robot buffer is nearly full");
		}
		else if (text.find("COMMAND_RESUME") != std::string::npos) {
			remoteBufferFull.store(false);
			LOG_INFO("Host resumed: Robot buffer has free space");
		}
		else {
			LOG_INFO("Hub stdout: %s", text.c_str());
		}
	}
	else if (type == REPLY_STATUS) {

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

/*const float radius_mm = 300.0f;
	const float feedrate_mm_per_min = 1200.0f; // was 600
	const int STEPS_PER_MM = 10;

	const float radius_steps = radius_mm * STEPS_PER_MM;
	const float feedrate_steps_per_sec = (feedrate_mm_per_min / 60.0f) * STEPS_PER_MM;
	const float circumference = 2.0f * 3.14159256f * radius_steps;

	const int NUM_SEGMENTS = 200; // was 40
	const float segment_duration_s = circumference / (feedrate_steps_per_sec * NUM_SEGMENTS);
	const uint16_t duration_ms = (uint16_t)(segment_duration_s * 1000.0f);
	const float angle_step = 2.0f * 3.14159265 / NUM_SEGMENTS;

	uint8_t packet[13];
	packet[0] = 0x06;
	packet[1] = 0x01;
	packet[2] = 0x01;

	int start_tx = (int)radius_steps;
	int start_ty = 0;
	uint16_t start_dur = 800;
	std::memcpy(&packet[3], &start_tx, sizeof(start_tx));
	std::memcpy(&packet[7], &start_ty, sizeof(start_ty));
	std::memcpy(&packet[11], &start_dur, sizeof(start_dur));
	if (!transport.write(pybricksCommandEvent, packet, sizeof(packet), true)) {
		LOG_ERROR("Failed to write start point");
		return false;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(start_dur));

	float angle = angle_step;
	for (int i = 1; i < NUM_SEGMENTS; ++i) {
		int tx = (int)(radius_steps * std::cos(angle));
		int ty = (int)(radius_steps * std::sin(angle));

		std::memcpy(&packet[3], &tx, sizeof(tx));
		std::memcpy(&packet[7], &ty, sizeof(ty));
		std::memcpy(&packet[11], &duration_ms, sizeof(duration_ms));

		if (!transport.write(pybricksCommandEvent, packet, sizeof(packet), true)) {
			LOG_ERROR("Failed to write segment %d");
			return false;
		}

		angle += angle_step;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	int final_tx = start_tx;
	int final_ty = start_ty;
	std::memcpy(&packet[3], &final_tx, sizeof(final_tx));
	std::memcpy(&packet[7], &final_ty, sizeof(final_ty));
	std::memcpy(&packet[11], &duration_ms, sizeof(duration_ms));
	transport.write(pybricksCommandEvent, packet, sizeof(packet), true);

	LOG_INFO("Motion test completed! %d segments sent.", NUM_SEGMENTS);
	return true;*/

struct Point {
	int x;
	int y;
};

struct LineSegment {
	int target_x;
	int target_y;
	uint16_t duration_ms;
};

bool Controller::sendLineSegment(int tx, int ty, uint16_t dur) {
	uint8_t packet[13];
	packet[0] = 0x06;
	packet[1] = 0x01;
	packet[2] = 0x01;
	std::memcpy(&packet[3], &tx, sizeof(tx));
	std::memcpy(&packet[7], &ty, sizeof(ty));
	std::memcpy(&packet[11], &dur, sizeof(dur));
	return transport.write(pybricksCommandEvent, packet, sizeof(packet), true);
}

double angleBetween(const Point& a, const Point& b, const Point& c) {
	double dx1 = b.x - a.x;
	double dy1 = b.y - a.y;
	double dx2 = c.x - b.x;
	double dy2 = c.y - b.y;
	double dot = dx1 * dx2 + dy1 * dy2;
	double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
	double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
	if (len1 < 1e-6 || len2 < 1e-6) {
		return 0.0;
	}
	double cosAngle = dot / (len1 * len2);
	if (cosAngle > 1.0) {
		cosAngle = 1.0;
	}
	if (cosAngle < -1.0) {
		cosAngle = -1.0;
	}
	return std::acos(cosAngle);
}

std::vector<LineSegment> pointsToLineSegments(
	const std::vector<Point>& simplified,
	double feedrate_steps_per_sec,
	uint16_t min_duration_ms,
	double acceleration)
{
	std::vector<LineSegment> segs;
	if (simplified.size() < 2) {
		return segs;
	}

	double currentFeedrate = feedrate_steps_per_sec;

	for (size_t i = 0; i + 1 < simplified.size(); ++i) {
		double dx = simplified[i + 1].x - simplified[i].x;
		double dy = simplified[i + 1].y - simplified[i].y;
		double length = std::sqrt(dx * dx + dy * dy);

		if (i > 0 && i + 2 < simplified.size()) {
			double angle = angleBetween(simplified[i - 1], simplified[i], simplified[i + 1]);
			if (angle > 0.52) {
				double factor = std::max(0.2, 1.0 - angle / 3.14159);
				currentFeedrate = feedrate_steps_per_sec * factor;
			}
			else {
				currentFeedrate = feedrate_steps_per_sec;
			}
		}
		else {
			currentFeedrate = feedrate_steps_per_sec;
		}

		double dur_sec = length / currentFeedrate;
		uint16_t dur = std::max((uint16_t)(dur_sec * 1000), min_duration_ms);
		segs.push_back({ simplified[i + 1].x, simplified[i + 1].y, dur });
	}
	return segs;
}

double pointToSegmentDist(const Point& p, const Point& a, const Point& b) {
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	double len2 = dx * dx + dy * dy;
	if (len2 == 0) {
		return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
	}
	double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
	t = std::max(0.0, std::min(1.0, t));
	double projx = a.x + t * dx;
	double projy = a.y + t * dy;
	return std::sqrt((p.x - projx) * (p.x - projx) + (p.y - projy) * (p.y - projy));
}

void simplifyRDP(const std::vector<Point>& pts, double epsilon, std::vector<Point>& result) {
	if (pts.size() < 2) {
		result = pts;
		return;
	}
	double maxDist = 0;
	size_t maxIdx = 0;
	const Point& first = pts.front(), & last = pts.back();
	for (size_t i = 1; i < pts.size() - 1; ++i) {
		double d = pointToSegmentDist(pts[i], first, last);
		if (d > maxDist) { maxDist = d; maxIdx = i; }
	}
	if (maxDist > epsilon) {
		std::vector<Point> left(pts.begin(), pts.begin() + maxIdx + 1);
		std::vector<Point> right(pts.begin() + maxIdx, pts.end());
		std::vector<Point> leftRes, rightRes;
		simplifyRDP(left, epsilon, leftRes);
		simplifyRDP(right, epsilon, rightRes);
		result = leftRes;
		result.insert(result.end(), rightRes.begin() + 1, rightRes.end());
	}
	else {
		result = { first, last };
	}
}

std::vector<Point> readSkeletonCsv(const std::string& filename) {
	std::vector<Point> pts;
	std::ifstream f(filename);
	if (!f) {
		return pts;
	}
	std::string line;
	std::getline(f, line);
	while (std::getline(f, line)) {
		std::istringstream iss(line);
		std::string token;
		int y; int x;
		if (std::getline(iss, token, ',')) {
			y = std::stoi(token);
		}
		if (std::getline(iss, token, ',')) {
			x = std::stoi(token);
		}
		pts.push_back({ x * 5, y * 5 });
	}
	return pts;
}

struct MotionLimits {
	double max_velocity; // steps / s
	double max_accel; // steps / s^2
	double junction_deviation = 2.0; // допустимое отклонение на стыке, шагов
	double junction_min_factor = 0.5; // минимальная скорость как доля от max_velocity
};

double computeJunctionVelocity(
	const Point& prev, const Point& curr, const Point& next,
	const MotionLimits& limits)
{
	double dx1 = curr.x - prev.x;
	double dy1 = curr.y - prev.y;
	double dx2 = next.x - curr.x;
	double dy2 = next.y - curr.y;
	double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
	double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
	if (len1 < 1e-6 || len2 < 1e-6) {
		return limits.max_velocity; // не 0, чтобы не останавливаться при вырожденных сегментах
	}

	double cos_angle = (dx1 * dx2 + dy1 * dy2) / (len1 * len2);
	cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
	double angle = std::acos(cos_angle);

	double half_angle = angle * 0.5;
	double cos_half = std::cos(half_angle);
	double denom = 1.0 - cos_half;
	if (denom < 1e-9) {
		return limits.max_velocity; // почти прямой участок
	}

	double v_sq = limits.max_accel * limits.junction_deviation * cos_half / denom;
	if (v_sq < 0) v_sq = 0;
	double v = std::sqrt(v_sq);

	// Ограничение сверху
	if (v > limits.max_velocity) { 
		v = limits.max_velocity; 
	}

	// Ограничение снизу, чтобы не было полной остановки
	double v_min = limits.junction_min_factor * limits.max_velocity;
	if (v < v_min) { 
		v = v_min;
	}

	return v;
}

// Основной планировщик скорости
std::vector<double> planVelocity(const std::vector<Point>& points, const MotionLimits& limits) {
	if (points.size() < 2) {
		return {};
	}

	size_t N = points.size() - 1; // количество сегментов
	std::vector<double> lengths(N);
	for (size_t i = 0; i < N; ++i) {
		double dx = points[i + 1].x - points[i].x;
		double dy = points[i + 1].y - points[i].y;
		lengths[i] = std::sqrt(dx * dx + dy * dy);
	}

	// единый массив скоростей в вершинах (v[0] - старт, v[1] - финиш)
	std::vector<double> v(N + 1, limits.max_velocity);
	v[0] = 0.0; // начальная скорость
	v[N] = 0.0; // конечная скорость

	// Junction velocity в промежуточных узлах
	for (size_t i = 1; i < N; ++i) {
		v[i] = computeJunctionVelocity(points[i - 1], points[i], points[i + 1], limits);
	}

	// Forward pass: ограничиваем скорость возможностью разогнаться
	for (size_t i = 0; i < N; ++i) {
		double reachable = std::sqrt(v[i] * v[i] + 2.0 * limits.max_accel * lengths[i]);
		if (v[i + 1] > reachable) {
			v[i + 1] = reachable;
		}
	}

	// Backward pass: ограничиваем скорость возможностью затормозить
	for (size_t i = N; i-- > 0; ) {
		double reachable = std::sqrt(v[i + 1] * v[i + 1] + 2.0 * limits.max_accel * lengths[i]);
		if (v[i] > reachable) {
			v[i] = reachable;
		}
	}

	// Вычисляем длительности сегментов
	std::vector<double> durations(N);
	for (size_t i = 0; i < N; ++i) {
		if (v[i] + v[i + 1] > 0) {
			durations[i] = 2.0 * lengths[i] / (v[i] + v[i + 1]);
		}
		else {
			durations[i] = 0.0;
		}
	}
	return durations;
}

std::vector<Point> resampleByDistance(const std::vector<Point>& points, double spacing) {
	if (points.size() < 2 || spacing <= 0) {
		return points;
	}

	// Вычисляем кумулятивные длины
	std::vector<double> cum_len(points.size(), 0.0);
	for (size_t i = 1; i < points.size(); ++i) {
		double dx = points[i].x - points[i - 1].x;
		double dy = points[i].y - points[i - 1].y;
		cum_len[i] = cum_len[i - 1] + std::sqrt(dx * dx + dy * dy);
	}
	double total_len = cum_len.back();
	if (total_len < spacing) {
		return points; // слишком короткая траектория - не ресемплируем
	}

	std::vector<Point> resampled;
	resampled.push_back(points.front());

	double target = spacing;
	size_t index = 0; // индекс текущего сегмента (index -> index + 1)
	while (target < total_len - 1e-9) {
		// Находим сегмент, содержащий target
		while (index + 1 < cum_len.size() && cum_len[index + 1] < target) {
			index++;
		}
		double seg_start = cum_len[index];
		double seg_end = cum_len[index + 1];
		double seg_len = seg_end - seg_start;
		if (seg_len < 1e-9) {
			target += spacing;
			continue;
		}
		double t = (target - seg_start) / seg_len;
		double x = points[index].x + (points[index + 1].x - points[index].x) * t;
		double y = points[index].y + (points[index + 1].y - points[index].y) * t;
		resampled.push_back({
			static_cast<int>(std::round(x)),
			static_cast<int>(std::round(y))
		});
		target += spacing;
	}

	// Добавляем конечную точку, если она не совпадает с последней добавленной
	if (resampled.back().x != points.back().x || resampled.back().y != points.back().y) {
		resampled.push_back(points.back());
	}

	return resampled;
}

std::vector<Point> smoothPoints(const std::vector<Point>& pts, int window = 2) {
	if (pts.size() < 3) {
		return pts;
	}
	std::vector<Point> smoothed = pts;
	for (int pass = 0; pass < 2; ++pass) {
		std::vector<Point> temp = smoothed;
		for (size_t i = 1; i < pts.size() - 1; ++i) {
			double sum_x = 0, sum_y = 0;
			int count = 0;
			for (int j = -window; j <= window; ++j) {
				int idx = i + j;
				if (idx < 0) {
					idx = 0;
				}
				if (idx >= (int)pts.size()) {
					idx = pts.size() - 1;
				}
				sum_x += smoothed[idx].x;
				sum_y += smoothed[idx].y;
				count++;
			}
			temp[i] = { static_cast<int>(std::round(sum_x / count)),
						static_cast<int>(std::round(sum_y / count)) };
		}
		smoothed = temp;
	}
	return smoothed;
}

std::vector<Point> smoothSharpCorners(const std::vector<Point>& pts, double angleThreshold = 0.4) {
	if (pts.size() < 3) {
		return pts;
	}
	std::vector<Point> smoothed = pts;
	for (size_t i = 1; i < pts.size() - 1; ++i) {
		double angle = angleBetween(pts[i - 1], pts[i], pts[i + 1]);
		if (angle > angleThreshold) {
			// Заменяем вершину на среднее с соседями (или используем дуговое сглаживание)
			smoothed[i].x = (pts[i - 1].x + pts[i].x + pts[i + 1].x) / 3;
			smoothed[i].y = (pts[i - 1].y + pts[i].y + pts[i + 1].y) / 3;
		}
	}
	return smoothed;
}

bool Controller::sendMotionBlock(const std::vector<MotionSegment>& segments) {
	if (segments.empty()) return true;

	const size_t SEGMENT_SIZE = 10; // 4 + 4 + 2
	size_t maxWrite = transport.getMaxWriteSize();
	if (maxWrite < 4) return false; // не хватает места для заголовка

	size_t maxSegmentsPerWrite = (maxWrite - 3) / SEGMENT_SIZE; // 3 байта: type, cmd, count
	if (maxSegmentsPerWrite == 0) return false;

	size_t offset = 0;
	while (offset < segments.size()) {
		size_t count = std::min(maxSegmentsPerWrite, segments.size() - offset);
		size_t packetSize = 3 + count * SEGMENT_SIZE;
		std::vector<uint8_t> buffer(packetSize);

		buffer[0] = 0x06;                // тип данных (как в sendLineSegment)
		buffer[1] = CMD_MOTION_BLOCK;    // команда
		buffer[2] = static_cast<uint8_t>(count);

		for (size_t i = 0; i < count; ++i) {
			const MotionSegment& seg = segments[offset + i];
			size_t base = 3 + i * SEGMENT_SIZE;
			memcpy(&buffer[base], &seg.target_x, 4);
			memcpy(&buffer[base + 4], &seg.target_y, 4);
			memcpy(&buffer[base + 8], &seg.duration_ms, 2);
		}

		if (!transport.write(pybricksCommandEvent, buffer.data(), buffer.size(), true)) {
			LOG_ERROR("Failed to send motion block at offset %zu", offset);
			return false;
		}

		offset += count;

		// Небольшая пауза между блоками, чтобы не переполнять BLE стек
		if (offset < segments.size()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}
	return true;
}

bool Controller::runMotionTest() {
	auto rawPoints = readSkeletonCsv("one_contour.csv");
	if (rawPoints.empty()) {
		LOG_ERROR("No points loaded");
		return false;
	}
	LOG_INFO("Loaded %d raw points", rawPoints.size());

	// Очистка от дубликатов
	std::vector<Point> cleaned;
	Point last = { -9999, -9999 };
	for (auto& p : rawPoints) {
		if (std::abs(p.x - last.x) + std::abs(p.y - last.y) >= 2) {
			cleaned.push_back(p);
			last = p;
		}
	}
	LOG_INFO("after cleaning: %d points", cleaned.size());

	// RDP с меньшим epsilon для сохранения деталей
	double epsilon = 3.0;
	std::vector<Point> simplified;
	simplifyRDP(cleaned, epsilon, simplified);
	LOG_INFO("After RDP: %d points", simplified.size());

	// Ресемплинг по расстоянию (равномерное распределение точек)
	double resample_spacing = 15.0; // шагов
	std::vector<Point> resampled = resampleByDistance(simplified, resample_spacing);
	LOG_INFO("After resampling: %d points", resampled.size());

	// Лёгкое сглаживание скользящим средним
	std::vector<Point> smoothed = smoothPoints(resampled, 2);
	LOG_INFO("After smoothing: %d points", smoothed.size());

	// Планировщик скорости
	MotionLimits limits;
	limits.max_velocity = 1200.0;
	limits.max_accel = 3000.0;
	limits.junction_deviation = 3.0;

	auto durations_sec = planVelocity(smoothed, limits);

	// Формируем сегменты с длительностями
	std::vector<MotionSegment> motionSegments;
	for (size_t i = 0; i < durations_sec.size(); ++i) {
		uint16_t dur_ms = static_cast<uint16_t>(durations_sec[i] * 1000.0);
		if (dur_ms < 10) dur_ms = 10; // поднимаем минимум, чтобы хаб успевал
		motionSegments.push_back({ smoothed[i + 1].x, smoothed[i + 1].y, dur_ms });
	}
	LOG_INFO("Generated %d motion segments", motionSegments.size());

	// Отправка стартовой точки с задержкой 800 мс
	if (!motionSegments.empty()) {
		int start_tx = smoothed[0].x;
		int start_ty = smoothed[0].y;
		uint16_t start_dur = 800;
		MotionSegment startSeg = { start_tx, start_ty, start_dur };
		std::vector<MotionSegment> startBlock = { startSeg };
		if (!sendMotionBlock(startBlock)) {
			LOG_ERROR("Failed to send start point");
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(start_dur));
	}

	// Отправка всех сегментов блоками с учётом flow control
	size_t sent = 0;
	while (sent < motionSegments.size()) {
		// Ждём, если хаб сообщил о переполнении буфера
		while (remoteBufferFull.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		// Определяем размер блока (не более maxSegmentsPerWrite)
		size_t maxWrite = transport.getMaxWriteSize();
		size_t maxSegments = (maxWrite - 3) / 10;
		if (maxSegments == 0) maxSegments = 1;
		size_t count = std::min(maxSegments, motionSegments.size() - sent);

		std::vector<MotionSegment> block(
			motionSegments.begin() + sent,
			motionSegments.begin() + sent + count
		);

		if (!sendMotionBlock(block)) {
			LOG_ERROR("Failed to send motion block starting at %zu", sent);
			return false;
		}

		sent += count;
		LOG_DEBUG("Sent block of %zu segments (total %zu/%zu)", count, sent, motionSegments.size());

		// Небольшая пауза между блоками
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	LOG_INFO("All %d segments sent in blocks.", motionSegments.size());
	return true;
}
