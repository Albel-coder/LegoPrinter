#pragma once

#include <cstdint>
#include <vector>

namespace motion {

    // Все скорости в шагах/с, ускорения в шагах/с^2, длины в шагах.
    struct PlannerConfig {
        double maxAccel = 8000.0; // шаги/с^2
        double junctionDeviation = 4.0;    // шагов — допуск скругления на стыке
        double maxVelocity = 2200.0; // шагов/с — глобальный потолок
        double junctionMinFactor = 0.0;    // 0 = физика решает сама
    };

    // Один отрезок XY в терминах планировщика.
    struct MoveSegment {
        int32_t dx = 0;
        int32_t dy = 0;
        double  length = 0.0;            // шагов
        double  requestedVelocity = 0.0; // шагов/с — лимит из F-кода
    };

    // Результат планирования одного сегмента.
    struct PlannedSegment {
        double vEntry = 0.0; // скорость на входе, шагов/с
        double vExit = 0.0; // скорость на выходе, шагов/с
        double vPeak = 0.0; // пиковая скорость внутри сегмента
        double durationSec = 0.0; // полное время проезда
    };

    // Разложение профиля по фазам — для диагностики.
    struct ProfileTiming {
        double duration = 0.0;
        double vPeak = 0.0;
        double tAcc = 0.0;
        double tCruise = 0.0;
        double tDec = 0.0;
    };

    class MotionPlanner {
    public:
        explicit MotionPlanner(const PlannerConfig& cfg);

        // Главный вход: планирует всю цепочку.
        std::vector<PlannedSegment> plan(const std::vector<MoveSegment>& segments) const;

        // Чистая функция — удобно юнит-тестировать.
        static ProfileTiming computeProfile(
            double length, double vIn, double vOut,
            double vMax, double maxAccel);

    private:
        PlannerConfig cfg;

        double junctionVelocity(const MoveSegment& a, const MoveSegment& b) const;
    };

} // namespace motion
