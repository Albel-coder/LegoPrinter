#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Компенсатор люфта для одной оси. Работает в шагах.
//
// Модель:
//   engagement ∈ [-half, +half] — насколько мотор "впереди" пера.
//   +half = мотор полностью выбран в положительную сторону,
//   -half = в отрицательную, 0 = по центру (люфт не выбран).
//
// При команде "перо должно сдвинуться на pen_delta":
//   target_eng = sign(pen_delta) * half
//   motor_delta = pen_delta + (target_eng - engagement)
//   engagement = target_eng
//
// Это гарантирует, что перо физически сдвинется на pen_delta, а мотор
// при смене направления сначала выберет люфт (extra = |target_eng - eng|).
//
// Опции:
//   min_move_steps — деадбенд: дельты меньше него не меняют engagement
//                    (1:1), чтобы шум/дрожание не дёргали мотор.
//                    По умолчанию 0 — компенсировать всегда.
//
// Начальный engagement = 0 (предполагаем, что стартуем "по центру").
class BacklashAxis {
public:
    BacklashAxis(double backlashMm, double stepsPerMm, double minMoveMm = 0.0)
        : half_(stepsToSteps(backlashMm, stepsPerMm) / 2)
        , minMove_(stepsToSteps(minMoveMm, stepsPerMm))
    {}

    // Возвращает моторную дельту; может быть больше penDelta по модулю.
    int32_t compensate(int32_t penDelta) {
        if (penDelta == 0 || half_ == 0) return penDelta;

        if (minMove_ > 0 && std::abs(penDelta) < minMove_) {
            return penDelta;
        }

        const int32_t dir = (penDelta > 0) ? 1 : -1;
        const int32_t targetEng = dir * half_;
        const int32_t motorDelta = penDelta + (targetEng - eng_);
        eng_ = targetEng;
        return motorDelta;
    }

private:
    static int32_t stepsToSteps(double mm, double stepsPerMm) {
        if (mm <= 0.0 || stepsPerMm <= 0.0) return 0;
        return static_cast<int32_t>(std::llround(mm * stepsPerMm));
    }

    int32_t half_;
    int32_t minMove_;
    int32_t eng_ = 0;
};

// Пара компенсаторов по X и Y.
class BacklashCompensator {
public:
    BacklashCompensator(double backlashXMm, double backlashYMm,
                        double stepsPerMmX, double stepsPerMmY,
                        double minMoveMm = 0.0)
        : x_(backlashXMm, stepsPerMmX, minMoveMm)
        , y_(backlashYMm, stepsPerMmY, minMoveMm)
    {}

    void apply(int32_t& dx, int32_t& dy) {
        dx = x_.compensate(dx);
        dy = y_.compensate(dy);
    }

private:
    BacklashAxis x_, y_;
};
