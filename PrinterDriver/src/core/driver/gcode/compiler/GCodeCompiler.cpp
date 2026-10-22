#include "GCodeCompiler.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace {
    constexpr double EPSILON = 1e-9;
    constexpr double MIN_SEGMENT_DURATION_MS = 4.0;

    bool nearlyEqual(double a, double b) {
        return std::abs(a - b) < EPSILON;
    }

    bool toUpperCommand(std::string& command) {
        if (command.empty()) return false;
        for (char& c : command) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return true;
    }
} // namespace

GCodeCompiler::GCodeCompiler(const PrinterConfig& printerConfig)
    : config(printerConfig)
{
    resetState();
}

void GCodeCompiler::resetState() {
    absolute = true;
    currentX = 0.0;
    currentY = 0.0;
    currentZ = 0.0;
    currentStepsX = 0;
    currentStepsY = 0;
    feedrate = config.defaultFeedrate;
    compiledCommandPacketCount = 0;
    feedrateG0 = config.maxFeedrate;
    feedrateG1 = config.defaultFeedrate;
}

std::string GCodeCompiler::trim(const std::string& value) {
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

// ============================================================
// compile
// ============================================================
bool GCodeCompiler::compile(const std::string& filename, MotionProgram& output) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open g-code: %s", filename.c_str());
        return false;
    }

    output = MotionProgram{};
    resetState();

    std::vector<RawParsedSegment> rawSegments;
    std::string line;

    // ============================================================
    // ЭТАП 1. ПАРСИНГ G-CODE
    // ============================================================
    while (std::getline(file, line)) {
        size_t commentPos = line.find(';');
        if (commentPos != std::string::npos) line.erase(commentPos);
        line = trim(line);
        if (line.empty()) continue;

        std::istringstream stream(line);
        std::string command;
        if (!(stream >> command)) continue;
        toUpperCommand(command);

        if (command == "G90") { absolute = true;  continue; }
        if (command == "G91") { absolute = false; continue; }
        if (command == "G21") continue;
        if (command == "M2" || command == "M30") break;

        if (command == "G0" || command == "G1") {
            double targetX = absolute ? currentX : 0.0;
            double targetY = absolute ? currentY : 0.0;
            double targetZ = absolute ? currentZ : 0.0;

            double newFeedrate = -1.0;

            std::string token;
            while (stream >> token) {
                char axis = token[0];
                double value = std::stod(token.substr(1));
                switch (axis) {
                case 'X': targetX = absolute ? value : currentX + value; break;
                case 'Y': targetY = absolute ? value : currentY + value; break;
                case 'Z': targetZ = absolute ? value : currentZ + value; break;
                case 'F': newFeedrate = value; break;
                }
            }

            if (newFeedrate > 0.0) {
                if (command == "G0") feedrateG0 = newFeedrate;
                else                 feedrateG1 = newFeedrate;
            }

            // --- Z-переход ---
            if (!nearlyEqual(targetZ, currentZ)) {
                int16_t zAngle = targetZ > currentZ
                    ? static_cast<int16_t>(config.zUpAngle)
                    : static_cast<int16_t>(config.zDownAngle);
                RawParsedSegment zSeg;
                zSeg.isZMove = true;
                zSeg.zAngle = zAngle;
                rawSegments.push_back(zSeg);
                currentZ = targetZ;
            }

            // --- XY-перемещение ---
            if (!nearlyEqual(targetX, currentX) || !nearlyEqual(targetY, currentY)) {
                int32_t targetStepsX = static_cast<int32_t>(
                    std::lround(targetX * config.stepsPerMmX));
                int32_t targetStepsY = static_cast<int32_t>(
                    std::lround(targetY * config.stepsPerMmY));

                const double stepsPerMm =
                    std::max(config.stepsPerMmX, config.stepsPerMmY);
                const double moveFeedrate = std::min(
                    (command == "G0") ? feedrateG0 : feedrateG1, config.maxFeedrate);
                const double velocityStepsSec =
                    (moveFeedrate / 60.0) * stepsPerMm;

                RawParsedSegment xySeg;
                xySeg.startPoint = { currentStepsX, currentStepsY };
                xySeg.endPoint = { targetStepsX, targetStepsY };
                xySeg.requestedVelocity = velocityStepsSec;
                xySeg.isZMove = false;
                rawSegments.push_back(xySeg);

                currentStepsX = targetStepsX;
                currentStepsY = targetStepsY;
                currentX = targetX;
                currentY = targetY;
            }
        }
    }

    if (rawSegments.empty()) {
        LOG_ERROR("No motion segments parsed");
        return false;
    }

    // ============================================================
    // ЭТАП 2. XY-СЕГМЕНТЫ ДЛЯ ПЛАНИРОВЩИКА
    // ============================================================
    std::vector<motion::MoveSegment> moves;
    moves.reserve(rawSegments.size());

    for (const auto& s : rawSegments) {
        if (s.isZMove) continue;
        motion::MoveSegment m;
        m.dx = s.endPoint.x - s.startPoint.x;
        m.dy = s.endPoint.y - s.startPoint.y;
        m.length = std::sqrt(
            static_cast<double>(m.dx) * m.dx +
            static_cast<double>(m.dy) * m.dy);
        m.requestedVelocity = s.requestedVelocity;
        moves.push_back(m);
    }

    if (moves.empty()) {
        LOG_ERROR("No XY segments after filtering");
        return false;
    }

    // ============================================================
    // ЭТАП 3. LOOK-AHEAD ПЛАНИРОВАНИЕ
    // ============================================================
    motion::PlannerConfig pcfg;
    pcfg.maxAccel = config.maxAccel;
    pcfg.junctionDeviation = config.junctionDeviation;
    pcfg.maxVelocity = config.maxVelocitySteps;
    pcfg.junctionMinFactor = config.junctionMinFactor;

    motion::MotionPlanner planner(pcfg);
    std::vector<motion::PlannedSegment> planned = planner.plan(moves);

    if (planned.size() != moves.size()) {
        LOG_ERROR("Planner returned wrong segment count: %zu vs %zu",
            planned.size(), moves.size());
        return false;
    }

    // ============================================================
    // ЭТАП 4. ДИАГНОСТИКА
    // ============================================================
    logPlannerDiagnostics(moves, planned);

    // ============================================================
    // ЭТАП 5. УПАКОВКА С КОМПЕНСАЦИЕЙ ОКРУГЛЕНИЯ
    // ============================================================
    size_t xyIndex = 0;
    double roundingErrorMs = 0.0;

    for (const auto& seg : rawSegments) {
        if (seg.isZMove) {
            if (!addZCommand(seg.zAngle, output)) {
                LOG_ERROR("Failed to add Z command");
                return false;
            }
        }
        else {
            // Компенсация накопленного округления времени
            double plannedMs = planned[xyIndex].durationSec * 1000.0
                + roundingErrorMs;
            double roundedMs = std::round(plannedMs);
            roundingErrorMs = plannedMs - roundedMs;

            if (roundedMs < MIN_SEGMENT_DURATION_MS) {
                roundingErrorMs += (MIN_SEGMENT_DURATION_MS - roundedMs);
                roundedMs = MIN_SEGMENT_DURATION_MS;
            }
            if (roundedMs > static_cast<double>(UINT16_MAX)) {
                LOG_ERROR("Segment duration out of uint16: %.2f ms "
                    "at index %zu", roundedMs, xyIndex);
                return false;
            }

            uint16_t durMs = static_cast<uint16_t>(roundedMs);

            int32_t dx = seg.endPoint.x - seg.startPoint.x;
            int32_t dy = seg.endPoint.y - seg.startPoint.y;

            if (dx < INT16_MIN || dx > INT16_MAX ||
                dy < INT16_MIN || dy > INT16_MAX) {
                LOG_ERROR("Delta out of int16: dx=%d dy=%d", dx, dy);
                return false;
            }

            if (!addXYSegment(dx, dy, durMs, output)) {
                LOG_ERROR("Failed to add XY segment %zu", xyIndex);
                return false;
            }
            xyIndex++;
        }
    }

    finalizeCommands(output);
    output.startThreshold = calculateStartThreshold(output.xySegmentCount);

    return true;
}

// ============================================================
// Упаковка сегментов в пакеты
// ============================================================

bool GCodeCompiler::addXYSegment(int32_t dx, int32_t dy,
    uint16_t durationMs,
    MotionProgram& program)
{
    if (!appendSegmentToPacket(static_cast<int16_t>(dx),
        static_cast<int16_t>(dy),
        durationMs, program)) {
        return false;
    }
    program.xySegmentCount++;
    program.estimatedDurationSec += static_cast<double>(durationMs) / 1000.0;
    return true;
}

bool GCodeCompiler::appendSegmentToPacket(int16_t dx, int16_t dy,
    uint16_t durationMs,
    MotionProgram& program)
{
    if (program.packets.empty() ||
        program.packets.back().segmentsCount >= MAX_SEGMENTS_PER_PACKET)
    {
        PreparedMotionPacket packet;
        packet.data[0] = MOTION_TRANSPORT_PREFIX;
        packet.data[1] = CMD_MOTION_BLOCK;
        packet.data[2] = 0;
        packet.size = MOTION_PACKET_HEADER_SIZE;
        packet.segmentsCount = 0;
        program.packets.push_back(packet);
    }

    PreparedMotionPacket& packet = program.packets.back();
    if (packet.segmentsCount >= MAX_SEGMENTS_PER_PACKET) return false;

    const size_t offset = MOTION_PACKET_HEADER_SIZE
        + static_cast<size_t>(packet.segmentsCount) * MOTION_SEGMENT_SIZE;
    if (offset + MOTION_SEGMENT_SIZE > MAX_BLE_PACKET_SIZE) return false;

    std::memcpy(packet.data.data() + offset, &dx, sizeof(dx));
    std::memcpy(packet.data.data() + offset + 2, &dy, sizeof(dy));
    std::memcpy(packet.data.data() + offset + 4, &durationMs, sizeof(durationMs));

    packet.segmentsCount++;
    packet.data[2] = packet.segmentsCount;
    packet.size = static_cast<uint8_t>(
        MOTION_PACKET_HEADER_SIZE
        + packet.segmentsCount * MOTION_SEGMENT_SIZE);

    return true;
}

// ============================================================
// Z-команды и финализация
// ============================================================

bool GCodeCompiler::addZCommand(int16_t angle, MotionProgram& program) {
    // Выталкиваем все накопленные XY-пакеты в команды
    while (compiledCommandPacketCount < program.packets.size()) {
        MotionCommand cmd;
        cmd.type = MotionCommandType::XY_PACKET;
        cmd.packetIndex = static_cast<uint16_t>(compiledCommandPacketCount);
        program.commands.push_back(cmd);
        compiledCommandPacketCount++;
    }

    MotionCommand zCmd;
    zCmd.type = MotionCommandType::Z_COMMAND;
    zCmd.zAngle = angle;
    zCmd.afterXY = static_cast<uint32_t>(program.xySegmentCount);
    program.commands.push_back(zCmd);
    return true;
}

void GCodeCompiler::finalizeCommands(MotionProgram& program) {
    while (compiledCommandPacketCount < program.packets.size()) {
        MotionCommand cmd;
        cmd.type = MotionCommandType::XY_PACKET;
        cmd.packetIndex = static_cast<uint16_t>(compiledCommandPacketCount);
        program.commands.push_back(cmd);
        compiledCommandPacketCount++;
    }
}

size_t GCodeCompiler::calculateStartThreshold(size_t segmentCount) const {
    if (segmentCount == 0) return 0;
    const size_t margin = config.startThresholdSafetyMargin;
    size_t threshold = (segmentCount > margin)
        ? segmentCount - margin
        : segmentCount;
    return std::min(threshold,
        static_cast<size_t>(config.maxStartThreshold));
}

// ============================================================
// Диагностика
// ============================================================

void GCodeCompiler::logPlannerDiagnostics(
    const std::vector<motion::MoveSegment>& moves,
    const std::vector<motion::PlannedSegment>& planned) const
{
    if (planned.empty()) return;

    double totalSec = 0.0;
    double minMs = 1e18, maxMs = 0.0, sumMs = 0.0;
    size_t below5 = 0, below10 = 0, below20 = 0, below50 = 0, above50 = 0;
    size_t fullSpeed = 0, junctionLimited = 0, feedrateLimited = 0;

    for (size_t i = 0; i < planned.size(); ++i) {
        const auto& p = planned[i];
        const double ms = p.durationSec * 1000.0;
        totalSec += p.durationSec;
        minMs = std::min(minMs, ms);
        maxMs = std::max(maxMs, ms);
        sumMs += ms;

        if (ms < 5.0)  ++below5;
        else if (ms < 10.0) ++below10;
        else if (ms < 20.0) ++below20;
        else if (ms < 50.0) ++below50;
        else                ++above50;

        const double vReq = moves[i].requestedVelocity;
        if (p.vPeak >= vReq * 0.99) {
            ++fullSpeed;
        }
        else if (p.vPeak < vReq - 1.0) {
            // Ограничение по ускорению/торможению/стыку
            ++junctionLimited;
        }
        else {
            ++feedrateLimited;
        }
    }

    LOG_INFO(
        "Planner: segments=%zu total=%.3f sec min=%.2fms max=%.2fms avg=%.2fms",
        planned.size(), totalSec, minMs, maxMs, sumMs / planned.size());

    LOG_INFO(
        "Duration hist: <5=%zu 5-10=%zu 10-20=%zu 20-50=%zu >50=%zu",
        below5, below10, below20, below50, above50);

    LOG_INFO(
        "Profile mix: fullSpeed=%zu junctionLimited=%zu feedrateLimited=%zu",
        fullSpeed, junctionLimited, feedrateLimited);

    // 10 самых длинных сегментов
    std::vector<size_t> idx(planned.size());
    for (size_t i = 0; i < planned.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        return planned[a].durationSec > planned[b].durationSec;
        });

    const size_t slowCount = std::min<size_t>(10, idx.size());
    LOG_INFO("--- %zu slowest segments ---", slowCount);
    for (size_t k = 0; k < slowCount; ++k) {
        const size_t i = idx[k];
        LOG_INFO(
            "SEG %zu: len=%.1f vIn=%.1f vOut=%.1f vPeak=%.1f dur=%.2fms req=%.1f",
            i, moves[i].length,
            planned[i].vEntry, planned[i].vExit, planned[i].vPeak,
            planned[i].durationSec * 1000.0,
            moves[i].requestedVelocity);
    }
}
