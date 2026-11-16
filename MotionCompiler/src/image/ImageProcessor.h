#pragma once

#include "../model/ImageProfile.h"

#include <opencv2/core.hpp>

#include <string>

class ImageProcessor {
public:
    enum class ColorMode { Red, Black, Dark };

    struct Config {
        // Какие пиксели считать "рисунком"
        ColorMode colorMode = ColorMode::Red;
        int blackThreshold = 128;
        int redHueLow = 10;
        int redHueHigh = 160;

        // Трансформация пиксель -> мм
        bool flipY = true;
        double originX = 6.0;
        double originY = 6.0;

        // Извлечение контуров
        int upscale = 8;
        int maxUpscaledSide = 6000;
        double minAreaPx2 = 3.0;
        double minAreaMm2 = 0.5;
        bool printHoles = true;
    };

    explicit ImageProcessor(Config config = {});

    // Загружает и нормализует в CV_8UC3 BGR (uint16 -> uint8, RGBA -> BGR с white-композитом).
    bool loadImage(const std::string& filename, cv::Mat& image) const;

    // Строит "поле рисования" в [0,1], CV_32F, размер = размер изображения.
    cv::Mat buildField(const cv::Mat& image) const;

    // Апскейлит поле, бинаризует, извлекает контуры через RETR_CCOMP,
    // конвертирует пиксели в мм, фильтрует по площади, резолвит parent-индексы.
    // В возвращаемых Contour заполнены и sourcePoints, и points (raw).
    ImageProfile extractContours(const cv::Mat& field, double scale) const;

private:
    Config config;
};
