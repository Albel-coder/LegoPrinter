#include "GCodeWriter.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

    std::string fmtCoord(double v) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.3f", v);
        return buf;
    }

    std::string fmtOne(double v) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f", v);
        return buf;
    }

    // Число без лишних нулей: 8000 -> "8000", 2.0 -> "2", 1.5 -> "1.500".
    std::string fmtNum(double v) {
        if (std::abs(v - std::round(v)) < 1e-9) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.0f", v);
            return buf;
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.3f", v);
        return buf;
    }

    class BacklashCompensator {
    public:
        BacklashCompensator(double backlashX, double backlashY,
            double initialX, double initialY) {
            half_[0] = backlashX / 2.0;
            half_[1] = backlashY / 2.0;
            motor_[0] = initialX;
            motor_[1] = initialY;
            pen_[0] = initialX;
            pen_[1] = initialY;
            lastDir_[0] = 0;
            lastDir_[1] = 0;
        }

        void moveTo(double x, double y, double& outX, double& outY) {
            outX = compAxis(0, x);
            outY = compAxis(1, y);
        }

        Point penPosition() const { return Point{ pen_[0], pen_[1] }; }

    private:
        double compAxis(int ax, double target) {
            const double delta = target - pen_[ax];
            if (std::abs(delta) < 1e-9) return motor_[ax];

            const int d = delta > 0 ? 1 : -1;
            double motorTarget;
            if (d == lastDir_[ax]) {
                motorTarget = motor_[ax] + delta;
            }
            else {
                motorTarget = target + d * half_[ax];
            }
            motor_[ax] = motorTarget;
            pen_[ax] = target;
            lastDir_[ax] = d;
            return motorTarget;
        }

        double half_[2], motor_[2], pen_[2];
        int lastDir_[2];
    };

    class Pen {
    public:
        Pen(std::vector<std::string>& out, const GCodeConfig& config)
            : out_(out), config_(config) {
        }

        void up() {
            if (state_ != State::Up) {
                zCommand(config_.zUpAngle, config_.zUpDwellMs, "pen up");
                state_ = State::Up;
            }
        }

        void down() {
            if (state_ != State::Down) {
                zCommand(config_.zDownAngle, config_.zDownDwellMs, "pen down");
                state_ = State::Down;
            }
        }

        bool isDown() const { return state_ == State::Down; }

    private:
        enum class State { Unknown, Up, Down };

        void zCommand(double angle, int dwellMs, const char* note) {
            if (!config_.zSyncCmd.empty()) {
                out_.push_back(config_.zSyncCmd);
            }

            std::string cmd = "G0 Z" + fmtNum(angle);
            if (config_.zFeed) {
                cmd += " F" + fmtNum(*config_.zFeed);
            }
            cmd += " ; ";
            cmd += note;
            out_.push_back(std::move(cmd));

            if (dwellMs > 0) {
                out_.push_back("G4 P" + std::to_string(dwellMs));
            }
            if (config_.zDownExtraMs > 0 && std::abs(angle - config_.zDownAngle) < 1e-9) {
                out_.push_back("G4 P" + std::to_string(config_.zDownExtraMs));
            }
            if (!config_.zSyncAfterCmd.empty()) {
                out_.push_back(config_.zSyncAfterCmd);
            }
        }

        std::vector<std::string>& out_;
        const GCodeConfig& config_;
        State state_ = State::Unknown;
    };

} // namespace

GCodeWriter::GCodeWriter(GCodeConfig config) : config(std::move(config)) {}

bool GCodeWriter::write(const ImageProfile& strokes, const std::string& filename) const {
    std::vector<std::string> out;

    out.push_back("; PNG -> " + fmtOne(config.widthMm) + "x" + fmtOne(config.heightMm)
        + " mm. RDP eps = " + fmtNum(config.rdpEpsilonMm) + " mm");
    out.push_back("; Backlash: X=" + fmtNum(config.backlashXMm)
        + " mm, Y=" + fmtNum(config.backlashYMm) + " mm");
    out.push_back("G21 ; mm");
    out.push_back("G90 ; absolute");

    if (config.emitG92Start) {
        out.push_back("G92 X" + fmtCoord(config.initialPosX)
            + " Y" + fmtCoord(config.initialPosY)
            + " ; force coord system to assumed start");
    }

    Pen pen(out, config);
    pen.up();

    BacklashCompensator comp(config.backlashXMm, config.backlashYMm,
        config.initialPosX, config.initialPosY);

    for (size_t k = 0; k < strokes.contours.size(); ++k) {
        const auto& P = strokes.contours[k].points;
        const size_t nPts = P.empty() ? 0 : P.size() - 1;
        out.push_back("; contour " + std::to_string(k) + " ("
            + std::to_string(nPts) + " points)");

        if (P.empty()) continue;

        const double sx = P.front().x;
        const double sy = P.front().y;
        const Point ppos = comp.penPosition();
        const double gap = std::hypot(sx - ppos.x, sy - ppos.y);

        if (pen.isDown() && gap <= config.minHopMm) {
            if (gap > 5e-4) {
                double mx, my;
                comp.moveTo(sx, sy, mx, my);
                out.push_back("G1 X" + fmtCoord(mx) + " Y" + fmtCoord(my)
                    + " F" + fmtNum(config.feedDraw));
            }
        }
        else {
            pen.up();
            double mx, my;
            comp.moveTo(sx, sy, mx, my);
            out.push_back("G0 X" + fmtCoord(mx) + " Y" + fmtCoord(my)
                + " F" + fmtNum(config.feedTravel));
            pen.down();
        }

        for (size_t i = 1; i + 1 < P.size(); ++i) {
            double mx, my;
            comp.moveTo(P[i].x, P[i].y, mx, my);
            out.push_back("G1 X" + fmtCoord(mx) + " Y" + fmtCoord(my)
                + " F" + fmtNum(config.feedDraw));
        }

        {
            double mx, my;
            comp.moveTo(sx, sy, mx, my);
            out.push_back("G1 X" + fmtCoord(mx) + " Y" + fmtCoord(my)
                + " F" + fmtNum(config.feedDraw) + " ; close contour");
        }
    }

    pen.up();
    out.push_back("M2");

    const ZVerification verify = verifyZ(out, config.zUpAngle, config.zDownAngle);
    if (!verify.errors.empty()) return false;

    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    for (const auto& line : out) {
        file << line << '\n';
    }
    return file.good();
}

GCodeWriter::ZVerification GCodeWriter::verifyZ(const std::vector<std::string>& lines,
    double zUpAngle,
    double zDownAngle) {
    ZVerification result;

    if (std::abs(zUpAngle - zDownAngle) < 1e-9) {
        result.errors.push_back("Z_UP_ANGLE and Z_DOWN_ANGLE are equal");
    }

    int zState = -1;  // -1 unknown, 0 up, 1 down

    for (size_t n = 0; n < lines.size(); ++n) {
        std::string s = lines[n];
        const auto semi = s.find(';');
        if (semi != std::string::npos) s.erase(semi);

        std::istringstream iss(s);
        std::string cmd;
        if (!(iss >> cmd)) continue;
        if (cmd != "G0" && cmd != "G1") continue;

        bool hasX = false, hasY = false, hasZ = false;
        double zVal = 0.0;

        std::string tok;
        while (iss >> tok) {
            if (tok.size() < 2) continue;
            if (!std::isalpha(static_cast<unsigned char>(tok[0]))) continue;
            const char axis = tok[0];
            double value = 0.0;
            try {
                value = std::stod(tok.substr(1));
            }
            catch (...) {
                continue;
            }
            if (axis == 'X') hasX = true;
            else if (axis == 'Y') hasY = true;
            else if (axis == 'Z') { hasZ = true; zVal = value; }
        }

        const bool hasXY = hasX || hasY;
        const std::string lineNo = "line " + std::to_string(n + 1);

        if (hasZ) {
            if (hasXY) {
                result.errors.push_back(lineNo + ": Z and XY in same command");
            }

            int newState;
            if (std::abs(zVal - zUpAngle) < 1e-9) newState = 0;
            else if (std::abs(zVal - zDownAngle) < 1e-9) newState = 1;
            else {
                result.errors.push_back(lineNo + ": unknown Z" + fmtNum(zVal));
                continue;
            }

            if (newState == zState) {
                result.errors.push_back(lineNo + ": repeated Z command");
            }

            if (newState == 0) ++result.ups;
            else ++result.downs;
            zState = newState;
        }
        else if (hasXY) {
            if (cmd == "G1") {
                ++result.g1;
                if (zState != 1) result.errors.push_back(lineNo + ": G1 with pen up");
            }
            else if (zState != 0) {
                result.errors.push_back(lineNo + ": G0 with pen down");
            }
        }
    }

    if (zState != 0) {
        result.errors.push_back("pen not up at end of file");
    }

    return result;
}
