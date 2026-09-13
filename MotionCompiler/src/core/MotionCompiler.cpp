#include "MotionCompiler.h"

#include "../gcode/GCodeWriter.h"
#include "../image/ImageProcessor.h"
#include "../path/ContourOrganizer.h"
#include "../path/PathSimplifier.h"

#include <fstream>

MotionCompiler::MotionCompiler() = default;

MotionCompiler::~MotionCompiler() = default;

bool MotionCompiler::compileImageProfiles(
    std::string inputFilename,
    std::string outputFilename,
    bool useSkeleton)
{
    ImageProcessor imageProcessor;

    cv::Mat image;

    if (!imageProcessor.loadImage(inputFilename, image)) {
        return false;
    }

    cv::Mat binary = imageProcessor.toBinary(image);

    if (binary.empty()) {
        return false;
    }

    ImageProfile profile = imageProcessor.extractProfile(binary, useSkeleton);

    if (profile.contours.empty()) {
        return false;
    }

    ContourOrganizer organizer;

    ImageProfile organized = organizer.organize(profile);

    if (organized.contours.empty()) {
        return false;
    }

    // --- ”прощение траектории ---

    SimplificationConfig simplifyConfig;

    if (useSkeleton) {
        // —келет Ч тонка€ лини€ ~1px.
        // —тупеньки сглаживаем, потом упрощаем агрессивно.
        simplifyConfig.smoothingWindow = 2;
        simplifyConfig.smoothingPasses = 1;
        simplifyConfig.rdpEpsilon = 1.5;
        simplifyConfig.deviationThreshold = 2.0;
    }
    else {
        // ќбычные контуры Ч без сглаживани€.
        // RDP и deviation уже работают хорошо.
        simplifyConfig.smoothingWindow = 0;
        simplifyConfig.smoothingPasses = 0;
        simplifyConfig.rdpEpsilon = 3.0;
        simplifyConfig.deviationThreshold = 3.0;
    }

    PathSimplifier simplifier(simplifyConfig);

    ImageProfile simplified = simplifier.simplify(organized);

    if (simplified.contours.empty()) {
        return false;
    }

    GCodeConfig config;

    config.travelSpeedMmMin = 2000.0;
    config.printSpeedMmMin = 500.0;
    config.liftHeightMm = 5.0;
    config.pixelsPerMm = 5.0;

    GCodeWriter writer(config);

    return writer.write(simplified, outputFilename);
}

bool MotionCompiler::compileCode(
    std::string inputFilename,
    std::string outputFilename)
{
    (void)inputFilename;
    (void)outputFilename;
    return false;
}
