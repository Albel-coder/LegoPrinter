#pragma once

#include "../image/ImageProcessor.h"
#include "../path/PathSimplifier.h"
#include "../path/ContourOrganizer.h"
#include "../gcode/GCodeWriter.h"

#include <optional>
#include <string>

class MotionCompiler {
public:
    struct Config {
        ImageProcessor::Config image;
        PathSimplificationConfig path;
        ContourOrganizer::Config organizer;
        GCodeConfig gcode;

        // Масштаб (мм на пиксель). Если задан outputWidthMm — scale вычисляется.
        double scale = 2.0;
        std::optional<double> outputWidthMm;

        // Превью
        std::string previewPath = "preview.png";
        bool generatePreview = true;
    };

    MotionCompiler();
    explicit MotionCompiler(Config config);
    ~MotionCompiler();

    // Полный конвейер: image -> field -> contours -> RDP -> order -> gcode.
    // При успехе (и при generatePreview) пишет PNG-превью.
    bool compileImageProfiles(const std::string& inputImage,
        const std::string& outputGcode) const;

    // Оставлено как заглушка для совместимости с C API.
    bool compileCode(const std::string& inputFilename,
        const std::string& outputFilename) const;

private:
    Config config;
};
