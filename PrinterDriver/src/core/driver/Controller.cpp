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
		LOG_INFO("Hub stdout: %s", text.c_str());

		// Перехватываем управление потоком
		if (text.find("FLOW_STOP") != std::string::npos) {
			remoteBufferFull.store(true);
			LOG_WARNING("Host paused: robot buffer is nearly full");
		}
		else if (text.find("FLOW_RESUME") != std::string::npos) {
			remoteBufferFull.store(false);
			LOG_INFO("Host resumed: robot buffer has free space");
		}
		else if (text.find("COMMAND_PAUSE") != std::string::npos) {
			remoteBufferFull.store(true);
			LOG_WARNING("Host paused: Robot buffer is nearly full");
		}
		else if (text.find("COMMAND_RESUME") != std::string::npos) {
			remoteBufferFull.store(false);
			LOG_INFO("Host resumed: Robot buffer has free space");
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

bool Controller::sendMotionBlock(const std::vector<MotionSegmentDelta>& segments) {
	if (segments.empty())
		return true;

	constexpr size_t SEGMENT_SIZE = 6;
	constexpr size_t HEADER_SIZE = 3;

	const size_t maxWrite =	transport.getMaxWriteSize();

	if (maxWrite < HEADER_SIZE + SEGMENT_SIZE)
		return false;

	const size_t maxSegmentsPerWrite =
		(maxWrite - HEADER_SIZE) / SEGMENT_SIZE;

	if (maxSegmentsPerWrite == 0)
		return false;

	size_t offset = 0;

	while (offset < segments.size()) {
		const size_t count = std::min(maxSegmentsPerWrite, segments.size() - offset);
		const size_t packetSize = HEADER_SIZE + count * SEGMENT_SIZE;

		std::vector<uint8_t> buffer(packetSize);
		buffer[0] = 0x06;
		buffer[1] = CMD_MOTION_BLOCK;
		buffer[2] =	static_cast<uint8_t>(count);

		for (size_t i = 0; i < count; ++i) {
			const auto& seg = segments[offset + i];
			const size_t base =	HEADER_SIZE + i * SEGMENT_SIZE;

			memcpy(&buffer[base], &seg.dx, sizeof(seg.dx));
			memcpy(&buffer[base + 2], &seg.dy, sizeof(seg.dy));
			memcpy(&buffer[base + 4], &seg.duration_ms,	sizeof(seg.duration_ms));
		}

		if (!transport.write(pybricksCommandEvent, buffer.data(), buffer.size(), true)) {
			LOG_ERROR("Failed to send motion block at offset %zu", offset);
			return false;
		}

		offset += count;

		if (offset < segments.size()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	}
	return true;
}

struct SlowSegment {
	size_t index;
	double duration_ms;
};

struct SegInfo {
	size_t index;
	double length;
	double duration_ms;
	double speed;
	double angle_deg;
	int x, y; // начальная точка
};

struct CornerInfo {
	size_t index;
	double angle_deg;
	Point prev;
	Point point;
	Point next;
};

// Находит самые острые углы (минимальный угол между сегментами)
std::vector<CornerInfo> findSharpestCorners(const std::vector<Point>& points) {
	std::vector<CornerInfo> result;
	if (points.size() < 3) {
		return result;
	}
	result.reserve(points.size() - 2);

	for (size_t i = 1; i + 1 < points.size(); ++i) {
		double angle = angleBetween(points[i - 1], points[i], points[i + 1]) * 180.0 / 3.141592653589793;
		result.push_back({ i, angle, points[i - 1], points[i], points[i + 1] });
	}

	std::sort(result.begin(), result.end(),
		[](const CornerInfo& a, const CornerInfo& b) {
			return a.angle_deg < b.angle_deg;
	});

	return result;
}

// Жадное упрощение: выбрасывает точки, если отклонение от новой прямой не превышает maxDeviation
std::vector<Point> simplifyByDeviation(const std::vector<Point>& points, double maxDeviation) {
	if (points.size() < 3) {
		return points;
	}

	std::vector<Point> result;
	result.reserve(points.size());

	size_t anchor = 0;
	result.push_back(points[anchor]);

	while (anchor + 1 < points.size()) {
		size_t best = anchor + 1;

		for (size_t candidate = anchor + 2; candidate < points.size(); ++candidate) {
			bool valid = true;
			for (size_t j = anchor + 1; j < candidate; ++j) {
				double deviation = pointToSegmentDist(points[j], points[anchor], points[candidate]);
				if (deviation > maxDeviation) {
					valid = false;
					break;
				}
			}
			if (!valid) {
				break;
			}
			best = candidate;
		}

		result.push_back(points[best]);
		anchor = best;
	}

	return result;
}

// Предварительная упаковка всех сегментов в пакеты
std::vector<PreparedMotionPacket> prepareMotionPackets(
	const std::vector<MotionSegmentDelta>& segments,
	size_t maxWrite)
{
	constexpr size_t HEADER_SIZE = 3;
	constexpr size_t SEGMENT_SIZE = 6;

	std::vector<PreparedMotionPacket> packets;
	if (segments.empty()) {
		return packets;
	}

	if (maxWrite > PreparedMotionPacket{}.data.size()) {
		maxWrite = PreparedMotionPacket{}.data.size();
	}

	if (maxWrite < HEADER_SIZE + SEGMENT_SIZE) {
		return packets;
	}

	const size_t maxSegmentsPerPacket = (maxWrite - HEADER_SIZE) / SEGMENT_SIZE;
	const size_t packetCount = (segments.size() + maxSegmentsPerPacket - 1) / maxSegmentsPerPacket;
	packets.reserve(packetCount);

	size_t offset = 0;
	while (offset < segments.size()) {
		const size_t count = std::min(maxSegmentsPerPacket, segments.size() - offset);

		PreparedMotionPacket packet;
		packet.data[0] = 0x06;
		packet.data[1] = CMD_MOTION_BLOCK;
		packet.data[2] = static_cast<uint8_t>(count);

		size_t writeOffset = HEADER_SIZE;
		for (size_t i = 0; i < count; ++i) {
			const MotionSegmentDelta& seg = segments[offset + i];
			std::memcpy(packet.data.data() + writeOffset, &seg.dx, sizeof(seg.dx));
			writeOffset += sizeof(seg.dx);
			std::memcpy(packet.data.data() + writeOffset, &seg.dy, sizeof(seg.dy));
			writeOffset += sizeof(seg.dy);
			std::memcpy(packet.data.data() + writeOffset, &seg.duration_ms, sizeof(seg.duration_ms));
			writeOffset += sizeof(seg.duration_ms);
		}

		packet.size = static_cast<uint8_t>(writeOffset);
		packet.segmentCount = static_cast<uint8_t>(count);
		packets.push_back(packet);
		offset += count;
	}

	return packets;
}

bool Controller::sendPreparedMotionPackets(
	const std::vector<PreparedMotionPacket>& packets)
{
	if (packets.empty()) {
		return true;
	}

	struct PacketTiming {
		double waitMs = 0.0;
		double writeMs = 0.0;
	};

	std::vector<PacketTiming> timings;
	timings.reserve(packets.size());

	size_t totalBytes = 0;
	size_t sentPackets = 0;
	size_t sentSegments = 0;

	const auto startTime = std::chrono::steady_clock::now();

	for (size_t i = 0; i < packets.size(); ++i) {
		const auto& packet = packets[i];

		// Замеряем ожидание буфера
		const auto waitStart = std::chrono::steady_clock::now();

		while (remoteBufferFull.load(std::memory_order_acquire)) {
			// Пока буфер полон, ждём.
			// Вместо yield можно использовать sleep(0) или sleep(1) для меньшего потребления CPU.
			std::this_thread::yield();
		}

		const auto waitEnd = std::chrono::steady_clock::now();
		double waitMs = std::chrono::duration<double, std::milli>(waitEnd - waitStart).count();

		// Замеряем сам write
		const auto writeStart = std::chrono::steady_clock::now();

		const bool ok = transport.write(
			pybricksCommandEvent,
			packet.data.data(),
			packet.size,
			true
		);

		const auto writeEnd = std::chrono::steady_clock::now();
		double writeMs = std::chrono::duration<double, std::milli>(writeEnd - writeStart).count();

		timings.push_back({ waitMs, writeMs });

		if (!ok) {
			LOG_ERROR(
				"Failed to send prepared packet %zu/%zu",
				i,
				packets.size()
			);
			return false;
		}

		totalBytes += packet.size;
		sentSegments += packet.segmentCount;
		++sentPackets;
	}

	const auto endTime = std::chrono::steady_clock::now();
	const double totalSeconds =
		std::chrono::duration<double>(endTime - startTime).count();

	// Агрегируем статистику
	double totalWaitMs = 0.0;
	double totalWriteMs = 0.0;
	double maxWaitMs = 0.0;
	double maxWriteMs = 0.0;

	for (const auto& t : timings) {
		totalWaitMs += t.waitMs;
		totalWriteMs += t.writeMs;
		maxWaitMs = std::max(maxWaitMs, t.waitMs);
		maxWriteMs = std::max(maxWriteMs, t.writeMs);
	}

	const double avgWaitMs = timings.empty() ? 0.0 : totalWaitMs / timings.size();
	const double avgWriteMs = timings.empty() ? 0.0 : totalWriteMs / timings.size();

	LOG_INFO(
		"Motion TX: packets=%zu segments=%zu total=%.3f sec "
		"packets/sec=%.1f segments/sec=%.1f bytes/sec=%.1f",
		sentPackets,
		sentSegments,
		totalSeconds,
		sentPackets / totalSeconds,
		sentSegments / totalSeconds,
		totalBytes / totalSeconds
	);

	LOG_INFO(
		"TX timing: avg wait=%.3f ms max wait=%.3f ms | "
		"avg write=%.3f ms max write=%.3f ms",
		avgWaitMs,
		maxWaitMs,
		avgWriteMs,
		maxWriteMs
	);

	// Выводим детальную информацию по каждому пакету (для отладки)
	for (size_t i = 0; i < timings.size(); ++i) {
		LOG_DEBUG(
			"TX packet %zu: wait=%.3f ms write=%.3f ms",
			i,
			timings[i].waitMs,
			timings[i].writeMs
		);
	}

	return true;
}

bool Controller::runMotionTest() {
	auto rawPoints = readSkeletonCsv("one_contour.csv");
	if (rawPoints.empty()) {
		LOG_ERROR("No points loaded");
		return false;
	}

	LOG_INFO("Loaded %zu raw points", rawPoints.size());

	// 1. Cleaning
	std::vector<Point> cleaned;
	cleaned.reserve(rawPoints.size());
	Point last = { -9999, -9999 };
	for (const auto& p : rawPoints) {
		if (std::abs(p.x - last.x) + std::abs(p.y - last.y) >= 2) {
			cleaned.push_back(p);
			last = p;
		}
	}
	LOG_INFO("After cleaning: %zu points", cleaned.size());

	if (cleaned.size() < 2) {
		LOG_ERROR("Not enough points after cleaning");
		return false;
	}

	// 2. RDP simplification
	constexpr double RDP_EPSILON = 4.0;
	std::vector<Point> simplified;
	simplified.reserve(cleaned.size());
	simplifyRDP(cleaned, RDP_EPSILON, simplified);
	LOG_INFO("After RDP: %zu points (%.1f%% of cleaned)",
		simplified.size(), 100.0 * simplified.size() / cleaned.size());

	if (simplified.size() < 2) {
		LOG_ERROR("Not enough points after RDP");
		return false;
	}

	// 3. Диагностика самых острых углов после RDP
	auto sharpCorners = findSharpestCorners(simplified);
	const size_t cornerLogCount = std::min<size_t>(10, sharpCorners.size());
	LOG_INFO("--- %zu sharpest corners after RDP ---", cornerLogCount);
	for (size_t i = 0; i < cornerLogCount; ++i) {
		const auto& c = sharpCorners[i];
		LOG_INFO("CORNER #%zu: angle=%.2f deg  (%d,%d) -> (%d,%d) -> (%d,%d)",
			c.index, c.angle_deg, c.prev.x, c.prev.y, c.point.x, c.point.y, c.next.x, c.next.y);
	}

	// 4. Дополнительное сжатие через deviation simplification
	constexpr double FAST_MODE_MAX_DEVIATION = 3.0;  // можно менять для экспериментов
	std::vector<Point> motionPath = simplifyByDeviation(simplified, FAST_MODE_MAX_DEVIATION);

	LOG_INFO("After deviation simplification: %zu points (removed %zu, deviation=%.2f)",
		motionPath.size(), simplified.size() - motionPath.size(), FAST_MODE_MAX_DEVIATION);

	if (motionPath.size() < 2) {
		LOG_ERROR("Not enough points after deviation simplification");
		return false;
	}

	LOG_INFO("Final motion path: %zu points, %zu segments", motionPath.size(), motionPath.size() - 1);

	// 5. Velocity planning
	MotionLimits limits;
	limits.max_velocity = 2200.0;
	limits.max_accel = 8000.0;
	limits.junction_deviation = 12.0;
	limits.junction_min_factor = 0.8;

	auto durations_sec = planVelocity(motionPath, limits);
	if (durations_sec.empty()) {
		LOG_ERROR("Motion planner returned no durations");
		return false;
	}

	if (durations_sec.size() + 1 != motionPath.size()) {
		LOG_ERROR("Invalid duration count: durations=%zu points=%zu",
			durations_sec.size(), motionPath.size());
		return false;
	}

	// 6. Статистика по длительностям
	double min_ms = 1e100, max_ms = 0.0, total_ms = 0.0;
	size_t below5 = 0, below10 = 0, from10to20 = 0, from20to50 = 0, above50 = 0;
	for (double d : durations_sec) {
		double ms = d * 1000.0;
		min_ms = std::min(min_ms, ms);
		max_ms = std::max(max_ms, ms);
		total_ms += ms;
		if (ms < 5.0) ++below5;
		else if (ms < 10.0) ++below10;
		else if (ms < 20.0) ++from10to20;
		else if (ms < 50.0) ++from20to50;
		else ++above50;
	}

	LOG_INFO("Planner stats: segments=%zu total=%.2f sec min=%.2fms max=%.2fms avg=%.2fms "
		"<5=%zu 5-10=%zu 10-20=%zu 20-50=%zu >=50=%zu",
		durations_sec.size(), total_ms / 1000.0, min_ms, max_ms,
		total_ms / durations_sec.size(), below5, below10, from10to20, from20to50, above50);

	// 7. Самые медленные по длительности сегменты
	std::vector<SlowSegment> slowest;
	slowest.reserve(durations_sec.size());
	for (size_t i = 0; i < durations_sec.size(); ++i)
		slowest.push_back({ i, durations_sec[i] * 1000.0 });
	std::sort(slowest.begin(), slowest.end(),
		[](const SlowSegment& a, const SlowSegment& b) { return a.duration_ms > b.duration_ms; });

	const size_t slowCount = std::min<size_t>(10, slowest.size());
	LOG_INFO("--- %zu slowest segments by duration ---", slowCount);
	for (size_t j = 0; j < slowCount; ++j) {
		const auto& s = slowest[j];
		const Point& a = motionPath[s.index];
		const Point& b = motionPath[s.index + 1];
		LOG_INFO("Slow segment #%zu: %.2f ms, (%d,%d) -> (%d,%d)",
			s.index, s.duration_ms, a.x, a.y, b.x, b.y);
	}

	// 8. Генерация delta-сегментов
	constexpr uint16_t MIN_SEGMENT_DURATION_MS = 5;
	std::vector<MotionSegmentDelta> deltaSegments;
	deltaSegments.reserve(durations_sec.size());
	double actual_total_ms = 0.0;

	for (size_t i = 0; i < durations_sec.size(); ++i) {
		double planned_ms = durations_sec[i] * 1000.0;
		uint16_t dur_ms = static_cast<uint16_t>(std::lround(planned_ms));
		if (dur_ms < MIN_SEGMENT_DURATION_MS) dur_ms = MIN_SEGMENT_DURATION_MS;

		int32_t dx = motionPath[i + 1].x - motionPath[i].x;
		int32_t dy = motionPath[i + 1].y - motionPath[i].y;

		if (dx < INT16_MIN || dx > INT16_MAX || dy < INT16_MIN || dy > INT16_MAX) {
			LOG_ERROR("Delta out of int16 range: dx=%d dy=%d", dx, dy);
			return false;
		}

		deltaSegments.push_back({ static_cast<int16_t>(dx), static_cast<int16_t>(dy), dur_ms });
		actual_total_ms += dur_ms;
	}

	LOG_INFO("Generated %zu delta segments", deltaSegments.size());
	LOG_INFO("Timing after quantization: planned=%.2f sec actual=%.2f sec difference=%.2f%%",
		total_ms / 1000.0, actual_total_ms / 1000.0,
		((actual_total_ms - total_ms) / total_ms) * 100.0);

	// 9. Проверка сумм дельт
	int64_t totalDx = 0, totalDy = 0;
	for (const auto& seg : deltaSegments) {
		totalDx += seg.dx;
		totalDy += seg.dy;
	}
	int64_t expectedDx = motionPath.back().x - motionPath.front().x;
	int64_t expectedDy = motionPath.back().y - motionPath.front().y;

	LOG_INFO("Delta verification: sum=(%lld,%lld) expected=(%lld,%lld)",
		totalDx, totalDy, expectedDx, expectedDy);
	if (totalDx != expectedDx || totalDy != expectedDy) {
		LOG_ERROR("Delta verification FAILED");
		return false;
	}
	LOG_INFO("Delta verification PASSED");

	// 10. Самые медленные сегменты по фактической скорости
	std::vector<SegInfo> segInfos;
	segInfos.reserve(durations_sec.size());
	for (size_t i = 0; i < durations_sec.size(); ++i) {
		const Point& p0 = motionPath[i];
		const Point& p1 = motionPath[i + 1];
		double dx = p1.x - p0.x;
		double dy = p1.y - p0.y;
		double length = std::sqrt(dx * dx + dy * dy);
		double dur_ms = durations_sec[i] * 1000.0;
		double speed = (dur_ms > 0.0) ? (length / dur_ms * 1000.0) : 0.0;
		double angle_deg = 0.0;
		if (i > 0 && i + 1 < motionPath.size()) {
			angle_deg = angleBetween(motionPath[i - 1], motionPath[i], motionPath[i + 1]) * 180.0 / 3.141592653589793;
		}
		segInfos.push_back({ i, length, dur_ms, speed, angle_deg, p0.x, p0.y });
	}

	std::sort(segInfos.begin(), segInfos.end(),
		[](const SegInfo& a, const SegInfo& b) { return a.speed < b.speed; });

	const size_t infoCount = std::min<size_t>(20, segInfos.size());
	LOG_INFO("--- 20 slowest segments by speed ---");
	for (size_t i = 0; i < infoCount; ++i) {
		const auto& s = segInfos[i];
		LOG_INFO("SEG %zu: (%.1f,%.1f) len=%.2f dur=%.2fms speed=%.1f angle=%.1f",
			s.index, static_cast<double>(s.x), static_cast<double>(s.y),
			s.length, s.duration_ms, s.speed, s.angle_deg);
	}

	LOG_INFO("--- Full simplification benchmark ---");

	constexpr double testDeviations[] = {
		3.0, 5.0, 8.0, 12.0, 16.0, 20.0, 25.0, 30.0, 35.0, 40.0
	};

	constexpr double AVG_WRITE_MS = 118.344;   // из последних измерений
	constexpr size_t SEGMENTS_PER_PACKET = 4;  // при maxWrite=32

	// Базовые показатели для исходной RDP-траектории (до deviation simplification)
	const size_t basePoints = simplified.size();
	const size_t baseSegments = basePoints > 1 ? basePoints - 1 : 0;
	const size_t basePackets = (baseSegments + SEGMENTS_PER_PACKET - 1) / SEGMENTS_PER_PACKET;
	const double baseTxSec = basePackets * AVG_WRITE_MS / 1000.0;

	// Планируем базовое время печати
	MotionLimits baseLimits = limits; // используем те же лимиты
	auto baseDurations = planVelocity(simplified, baseLimits);
	double basePrintSec = 0.0;
	for (double d : baseDurations) basePrintSec += d;

	LOG_INFO("Baseline: %zu pts, %zu seg, %zu pkt, TX=%.3f sec, print=%.3f sec, margin=%+.3f sec",
		basePoints, baseSegments, basePackets, baseTxSec, basePrintSec,
		basePrintSec - baseTxSec);

	for (double deviation : testDeviations) {
		auto tempPath = simplifyByDeviation(simplified, deviation);
		size_t points = tempPath.size();
		size_t segments = points > 1 ? points - 1 : 0;
		size_t packets = (segments + SEGMENTS_PER_PACKET - 1) / SEGMENTS_PER_PACKET;
		double txSec = packets * AVG_WRITE_MS / 1000.0;

		// Планируем время печати для этого варианта
		auto durations = planVelocity(tempPath, limits);
		double printSec = 0.0;
		for (double d : durations) printSec += d;

		double marginSec = printSec - txSec;
		double marginPercent = printSec > 0.0 ? marginSec * 100.0 / printSec : 0.0;

		LOG_INFO("dev=%5.1f -> %3zu pts, %3zu seg, %2zu pkt, TX=%.3f sec, print=%.3f sec, margin=%+.3f sec (%+.1f%%)",
			deviation, points, segments, packets, txSec, printSec, marginSec, marginPercent);
	}

	return true;

	// 11. Подготовка BLE‑пакетов заранее
	const size_t maxWrite = transport.getMaxWriteSize();
	if (maxWrite < 9) {
		LOG_ERROR("BLE write size too small: %zu", maxWrite);
		return false;
	}

	auto prepareStart = std::chrono::steady_clock::now();
	std::vector<PreparedMotionPacket> packets = prepareMotionPackets(deltaSegments, maxWrite);
	auto prepareEnd = std::chrono::steady_clock::now();

	if (packets.empty()) {
		LOG_ERROR("Failed to prepare motion packets");
		return false;
	}

	double prepareMs = std::chrono::duration<double, std::milli>(prepareEnd - prepareStart).count();
	LOG_INFO("Prepared %zu BLE packets (%zu segments) in %.3f ms",
		packets.size(), deltaSegments.size(), prepareMs);

	// 12. Отправка стартовой точки
	const int start_tx = motionPath.front().x;
	const int start_ty = motionPath.front().y;
	constexpr uint16_t START_DURATION_MS = 800;

	LOG_INFO("Moving to start point: X=%d Y=%d", start_tx, start_ty);
	if (!sendLineSegment(start_tx, start_ty, START_DURATION_MS)) {
		LOG_ERROR("Failed to send start point");
		return false;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(START_DURATION_MS));

	// 13. Отправка подготовленных пакетов (без лишних задержек)
	if (!sendPreparedMotionPackets(packets)) {
		LOG_ERROR("Failed to send prepared motion packets");
		return false;
	}

	LOG_INFO("All %zu motion segments sent successfully", deltaSegments.size());
	return true;
}
