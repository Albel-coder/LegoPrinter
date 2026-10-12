#include "MotionPlanner.h"

#include <algorithm>
#include <cmath>

namespace motion {

    MotionPlanner::MotionPlanner(const PlannerConfig& c) : cfg(c) {}

    double MotionPlanner::junctionVelocity(
        const MoveSegment& a, const MoveSegment& b) const
    {
        if (a.length < 1e-9 || b.length < 1e-9) {
            return cfg.maxVelocity;
        }

        const double u1x = a.dx / a.length;
        const double u1y = a.dy / a.length;
        const double u2x = b.dx / b.length;
        const double u2y = b.dy / b.length;

        double cosAngle = u1x * u2x + u1y * u2y;
        cosAngle = std::max(-1.0, std::min(1.0, cosAngle));

        const double vCap = std::min({
            a.requestedVelocity,
            b.requestedVelocity,
            cfg.maxVelocity
            });

        const double angle = std::acos(cosAngle);
        if (angle < 1e-6) {
            return vCap; // прямая
        }

        const double cosHalf = std::cos(angle * 0.5);
        const double denom = 1.0 - cosHalf;
        if (denom < 1e-9) {
            return vCap;
        }

        // Формула Klipper/Grbl:
        //   v^2 = a * jd * cos(θ/2) / (1 - cos(θ/2))
        double vSq = cfg.maxAccel * cfg.junctionDeviation * cosHalf / denom;
        if (vSq < 0.0) vSq = 0.0;

        double v = std::sqrt(vSq);
        v = std::min(v, vCap);

        if (cfg.junctionMinFactor > 0.0) {
            const double vMin = cfg.junctionMinFactor * cfg.maxVelocity;
            v = std::max(v, std::min(vMin, vCap));
        }
        return v;
    }

    ProfileTiming MotionPlanner::computeProfile(
        double length, double vIn, double vOut,
        double vMax, double maxAccel)
    {
        ProfileTiming p{};
        if (length < 1e-9 || maxAccel < 1e-9) {
            return p;
        }

        vIn = std::min(vIn, vMax);
        vOut = std::min(vOut, vMax);

        const double sAcc = (vMax * vMax - vIn * vIn) / (2.0 * maxAccel);
        const double sDec = (vMax * vMax - vOut * vOut) / (2.0 * maxAccel);

        if (sAcc + sDec <= length) {
            // Трапеция
            p.vPeak = vMax;
            p.tAcc = (vMax - vIn) / maxAccel;
            p.tDec = (vMax - vOut) / maxAccel;
            const double sCruise = length - sAcc - sDec;
            p.tCruise = (vMax > 1e-9) ? (sCruise / vMax) : 0.0;
        }
        else {
            // Треугольник
            const double vp2 = 0.5 * (2.0 * maxAccel * length
                + vIn * vIn + vOut * vOut);
            p.vPeak = std::sqrt(std::max(0.0, vp2));
            p.tAcc = (p.vPeak > vIn) ? ((p.vPeak - vIn) / maxAccel) : 0.0;
            p.tDec = (p.vPeak > vOut) ? ((p.vPeak - vOut) / maxAccel) : 0.0;
            p.tCruise = 0.0;
        }

        p.duration = p.tAcc + p.tCruise + p.tDec;
        return p;
    }

    std::vector<PlannedSegment> MotionPlanner::plan(
        const std::vector<MoveSegment>& segments) const
    {
        std::vector<PlannedSegment> result;
        const size_t N = segments.size();
        if (N == 0) return result;

        // v[i] — скорость в вершине i, i = 0..N
        std::vector<double> v(N + 1, cfg.maxVelocity);
        v[0] = 0.0; // старт с места
        v[N] = 0.0; // финиш — полная остановка

        // 1. Junction velocity
        for (size_t i = 1; i < N; ++i) {
            v[i] = junctionVelocity(segments[i - 1], segments[i]);
        }

        // 2. Forward pass
        for (size_t i = 0; i < N; ++i) {
            const double vReach = std::sqrt(
                v[i] * v[i] + 2.0 * cfg.maxAccel * segments[i].length);

            const double vLimit = std::min(
                segments[i].requestedVelocity, cfg.maxVelocity);

            v[i + 1] = std::min({ v[i + 1], vReach, vLimit });
        }

        // 3. Backward pass
        for (size_t i = N; i-- > 0; ) {
            const double vReach = std::sqrt(
                v[i + 1] * v[i + 1] + 2.0 * cfg.maxAccel * segments[i].length);
            if (v[i] > vReach) v[i] = vReach;
        }

        // 4. Профиль на каждый сегмент
        result.resize(N);
        for (size_t i = 0; i < N; ++i) {
            const double vMax = std::min(
                segments[i].requestedVelocity, cfg.maxVelocity);

            const ProfileTiming p = computeProfile(
                segments[i].length, v[i], v[i + 1], vMax, cfg.maxAccel);

            result[i].vEntry = v[i];
            result[i].vExit = v[i + 1];
            result[i].vPeak = p.vPeak;
            result[i].durationSec = p.duration;
        }

        return result;
    }

} // namespace motion
