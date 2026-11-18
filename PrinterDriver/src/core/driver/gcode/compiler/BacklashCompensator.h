#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Компенсатор люфта для одной оси. Работает в шагах.
//
// Ключевые улучшения относительно старой реализации:
//   * Гистерезис направления: реальный флип только после |pendingAccum| >= flipThreshold.
//     Устраняет дребезг ±1-2 шага на почти-прямых участках.
//   * extra() возвращает люфтовую компоненту (motorDelta - penDelta),
//     чтобы вызывающий код мог её распределить по под-сегментам.
//
// Модель: eng ∈ [-H, +H], pen = motor - eng.
//   При движении пера в +x: targetEng = +H, motorDelta = penDelta + (targetEng - eng).
class BacklashAxis {
public:
    BacklashAxis(double backlashMm, double stepsPerMm, double minFlipMm)
        : half_(mmToSteps(backlashMm, stepsPerMm))
        , flipThreshold_(mmToSteps(minFlipMm, stepsPerMm))
    {
    }

    // Возвращает моторную дельту. После вызова extra() содержит
    // (motorDelta - penDelta) — люфтовую компоненту, которую нужно
    // распределить по под-сегментам (если их будет несколько).
    int32_t compensate(int32_t penDelta) {
        extraOut_ = 0;
        if (penDelta == 0) return 0;

        const int32_t dir = (penDelta > 0) ? 1 : -1;

        if (half_ == 0) {
            committedDir_ = dir;
            return penDelta;
        }

        if (committedDir_ == 0) {
            committedDir_ = dir;
            return commitFlip(penDelta, dir);
        }

        if (dir == committedDir_) {
            pendingAccum_ = 0;
            return penDelta;
        }

        // Движение против зафиксированного направления
        pendingAccum_ += penDelta;

        if (flipThreshold_ > 0 && std::abs(pendingAccum_) < flipThreshold_) {
            return penDelta;  // шум, не компенсируем
        }

        committedDir_ = dir;
        pendingAccum_ = 0;
        return commitFlip(penDelta, dir);
    }

    int32_t extra() const { return extraOut_; }

private:
    int32_t commitFlip(int32_t penDelta, int32_t dir) {
        const int32_t targetEng = dir * half_;
        const int32_t motorDelta = penDelta + (targetEng - eng_);
        extraOut_ = motorDelta - penDelta;
        eng_ = targetEng;
        return motorDelta;
    }

    static int32_t mmToSteps(double mm, double stepsPerMm) {
        if (mm <= 0.0 || stepsPerMm <= 0.0) return 0;
        return static_cast<int32_t>(std::llround(mm * stepsPerMm));
    }

    int32_t half_;
    int32_t flipThreshold_;
    int32_t eng_ = 0;
    int32_t committedDir_ = 0;
    int32_t pendingAccum_ = 0;
    int32_t extraOut_ = 0;
};
