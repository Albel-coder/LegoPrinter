#pragma once

#include "../model/ImageProfile.h"

#include <vector>

class ContourOrganizer {
public:
    enum class OrderMode { Reading, Nearest };

    struct Config {
        OrderMode order = OrderMode::Reading;
    };

    explicit ContourOrganizer(Config config = {});

    // Возвращает новый профиль, где каждый контур — замкнутый "штрих"
    // (первая точка продублирована в конце), повернутый так,
    // чтобы начинаться в ближайшей к предыдущему концу точке.
    // flipY должен совпадать с ImageProcessor::Config::flipY.
    ImageProfile planStrokes(const ImageProfile& profile, bool flipY) const;

private:
    std::vector<size_t> readingOrder(const ImageProfile& profile, bool flipY) const;

    Config config;
};
