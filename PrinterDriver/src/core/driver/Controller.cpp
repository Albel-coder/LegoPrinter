#include "Controller.h"
#include "protocol/PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

constexpr uint8_t CMD_MOVE_BLOCK = 0x13;
constexpr uint8_t CMD_SYNC_START = 0x11;
constexpr uint8_t CMD_SET_LIMITS = 0x20;
constexpr uint8_t CMD_RESET_POS = 0x21;
constexpr uint8_t CMD_GET_STATUS = 0x30;
constexpr uint8_t CMD_EMERGENCY_STOP = 0x40;
constexpr uint8_t CMD_PING = 0x41;
constexpr uint8_t CMD_CLEAR_BUFFER = 0x42;
constexpr uint8_t CMD_ENABLE_WATCHDOG = 0x50;
constexpr uint8_t CMD_SET_BACKLASH = 0x60;

constexpr uint8_t AXIS_ALL = 0xFF;
constexpr uint8_t REPLY_STATUS = 0x80;
constexpr uint8_t REPLY_PONG = 0x81;
constexpr uint8_t REPLY_ERROR = 0xFF;

constexpr uint8_t SEG_FLAG_STOP = 0x02;

constexpr float STEPS_PER_MM = 10.0f;

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

		// ѕерехватываем управление потоком
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
	double junction_deviation = 2.0;
};

// м€гкое ограничение скорости в узле на основе угла поворота
double computeJunctionVelocity(
	const Point& prev, const Point& curr, const Point& next, const MotionLimits& limits)
{
	double dx1 = curr.x - prev.x;
	double dy1 = curr.y - prev.y;
	double dx2 = next.x - curr.x;
	double dy2 = next.y - curr.y;
	double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
	double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
	if (len1 < 1e-6 || len2 < 1e-6) {
		return 0.0;
	}

	double cos_angle = (dx1 * dx2 + dy1 * dy2) / (len1 * len2);
	cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
	double angle = std::acos(cos_angle);

	// turn = 0 (пр€ма€) ... 1 (разворот 180)
	double turn = (1.0 - cos_angle) * 0.5;
	// ѕлавна€ крива€: при малых turn скорость почти не снижаетс€
	// при turn устремл€ющихс€ к 1 снижаетс€ до нул€
	double factor = 1.0 - turn * turn;
	return limits.max_velocity * factor;
}

// ќсновной планировщик скорости
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
	v[0] = 0.0; // начальна€ скорость
	v[N] = 0.0; // конечна€ скорость

	// Junction velocity в промежуточных узлах
	for (size_t i = 1; i < N; ++i) {
		v[i] = computeJunctionVelocity(points[i - 1], points[i], points[i + 1], limits);
	}

	// Forward pass: ограничиваем скорость возможностью разогнатьс€
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

	// ¬ычисл€ем длительности сегментов
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

	// ¬ычисл€ем кумул€тивные длины
	std::vector<double> cum_len(points.size(), 0.0);
	for (size_t i = 1; i < points.size(); ++i) {
		double dx = points[i].x - points[i - 1].x;
		double dy = points[i].y - points[i - 1].y;
		cum_len[i] = cum_len[i - 1] + std::sqrt(dx * dx + dy * dy);
	}
	double total_len = cum_len.back();
	if (total_len < spacing) {
		return points; // слишком коротка€ траектори€ - не ресемплируем
	}

	std::vector<Point> resampled;
	resampled.push_back(points.front());

	double target = spacing;
	size_t index = 0; // индекс текущего сегмента (index -> index + 1)
	while (target < total_len - 1e-9) {
		// Ќаходим сегмент, содержащий target
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

	// ƒобавл€ем конечную точку, если она не совпадает с последней добавленной
	if (resampled.back().x != points.back().x || resampled.back().y != points.back().y) {
		resampled.push_back(points.back());
	}

	return resampled;
}

bool Controller::runMotionTest() {
	auto rawPoints = readSkeletonCsv("one_contour.csv");
	if (rawPoints.empty()) {
		LOG_ERROR("No points loaded");
		return false;
	}
	LOG_INFO("Loaded %d raw points", rawPoints.size());

	std::vector<Point> cleaned;
	Point last = { -9999, -9999 };
	for (auto& p : rawPoints) {
		if (std::abs(p.x - last.x) + std::abs(p.y - last.y) >= 2) {
			cleaned.push_back(p);
			last = p;
		}
	}
	LOG_INFO("after cleaning: %d points", cleaned.size());

	// ѕосле RDP
	double epsilon = 10.0;
	std::vector<Point> simplified;
	simplifyRDP(cleaned, epsilon, simplified);
	LOG_INFO("After RDP: %d points", simplified.size());

	// Ќовый шаг: ресемплинг
	double resample_spacing = 10.0; // можно сделать параметром
	std::vector<Point> resampled = resampleByDistance(simplified, resample_spacing);
	LOG_INFO("After resampling: %d points", resampled.size());

	// ƒалее используем resampled вместо simplified
	MotionLimits limits;
	limits.max_velocity = 800.0;
	limits.max_accel = 2000.0;
	limits.junction_deviation = 2.0; // новое поле

	auto durations_sec = planVelocity(resampled, limits);

	std::vector<LineSegment> segments;
	for (size_t i = 0; i < durations_sec.size(); ++i) {
		uint16_t dur_ms = static_cast<uint16_t>(durations_sec[i] * 1000.0);
		if (dur_ms < 10) { // уменьшаем минимальную длительность
			dur_ms = 10;
		}
		segments.push_back({ resampled[i + 1].x, resampled[i + 1].y, dur_ms });
	}

	LOG_INFO("Generated %d elements", segments.size());

	if (!segments.empty()) {
		int start_tx = simplified[0].x;
		int start_ty = simplified[0].y;
		if (!sendLineSegment(start_tx, start_ty, 800)) {
			LOG_ERROR("Failed to send start point");
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(800));
	}

	size_t n = segments.size();
	if (n > 0) {
		// —татистика длительностей
		uint16_t min_dur = std::numeric_limits<uint16_t>::max();
		uint16_t max_dur = 0;
		double total_dur = 0;
		for (const auto& seg : segments) {
			total_dur += seg.duration_ms;
			if (seg.duration_ms < min_dur) {
				min_dur = seg.duration_ms;
			}
			if (seg.duration_ms > max_dur) {
				max_dur = seg.duration_ms;
			}
		}
		double avg_dur = total_dur / n;
		LOG_INFO("Motion stats: segments=%zu, total_time=%.2f s, min_dur=%u ms, max_dur=%u ms, avg_dur=%.2f ms",
			n, total_dur / 1000.0, min_dur, max_dur, avg_dur);

		// —татистика длин сегментов (используем simplified/resampled точки)
		// ѕредполагаем, что у нас есть вектор resampled (смотри следующий раздел)
		double min_len = std::numeric_limits<double>::max();
		double max_len = 0;
		double total_len = 0;
		for (size_t i = 0; i + 1 < resampled.size(); ++i) {
			double dx = resampled[i + 1].x - resampled[i].x;
			double dy = resampled[i + 1].y - resampled[i].y;
			double len = std::sqrt(dx * dx + dy * dy);
			min_len = std::min(min_len, len);
			max_len = std::max(max_len, len);
			total_len += len;
		}
		double avg_len = total_len / (resampled.size() - 1);
		LOG_INFO("Geometry stats: points=%zu, segment_lengths: min=%.2f, max=%.2f, avg=%.2f",
			resampled.size(), min_len, max_len, avg_len);
	}

	for (size_t i = 0; i < segments.size(); ++i) {
		const LineSegment& seg = segments[i];
		if (!sendLineSegment(seg.target_x, seg.target_y, seg.duration_ms)) {
			LOG_ERROR("Failed to send segment %d", i);
			return false;
		}

		//std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	LOG_INFO("All %d segments sent. Printer should be moving", segments.size());
	return true;
}
