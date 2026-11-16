#include "MotionCompiler.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace {

    void generatePreview(const ImageProfile& contours,
        double wMm, double hMm,
        const std::string& path) {
        const int ppm = 4;
        const int canvasW = static_cast<int>(wMm * ppm) + 20;
        const int canvasH = static_cast<int>(hMm * ppm) + 20;
        if (canvasW <= 0 || canvasH <= 0) return;

        cv::Mat canvas(canvasH, canvasW, CV_8UC3, cv::Scalar(255, 255, 255));

        for (const auto& c : contours.contours) {
            if (c.points.empty()) continue;

            std::vector<cv::Point> px;
            px.reserve(c.points.size());
            for (const auto& p : c.points) {
                const int x = static_cast<int>(std::round(p.x * ppm)) + 10;
                const int y = static_cast<int>(std::round((hMm - p.y) * ppm)) + 10;
                px.emplace_back(x, y);
            }

            if (px.size() >= 2) {
                cv::polylines(canvas,
                    std::vector<std::vector<cv::Point>>{px},
                    true, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
            }
            for (const auto& p : px) {
                cv::circle(canvas, p, 2, cv::Scalar(0, 0, 255), -1);
            }
        }

        cv::imwrite(path, canvas);
    }

} // namespace

MotionCompiler::MotionCompiler() : config{} {}
MotionCompiler::MotionCompiler(Config config) : config(std::move(config)) {}
MotionCompiler::~MotionCompiler() = default;

bool MotionCompiler::compileImageProfiles(const std::string& inputImage,
    const std::string& outputGcode) const {
    ImageProcessor imageProcessor(config.image);

    cv::Mat image;
    if (!imageProcessor.loadImage(inputImage, image)) return false;

    const double scale = config.outputWidthMm
        ? *config.outputWidthMm / static_cast<double>(image.cols)
        : config.scale;
    const double wMm = static_cast<double>(image.cols) * scale;
    const double hMm = static_cast<double>(image.rows) * scale;

    cv::Mat field = imageProcessor.buildField(image);
    ImageProfile profile = imageProcessor.extractContours(field, scale);
    if (profile.contours.empty()) return false;

    PathSimplifier simplifier(config.path);
    ImageProfile simplified = simplifier.simplify(profile);

    ContourOrganizer organizer(config.organizer);
    ImageProfile strokes = organizer.planStrokes(simplified, config.image.flipY);
    if (strokes.contours.empty()) return false;

    GCodeConfig gcodeCfg = config.gcode;
    gcodeCfg.widthMm = wMm;
    gcodeCfg.heightMm = hMm;
    gcodeCfg.rdpEpsilonMm = config.path.rdpEpsilonMm;

    GCodeWriter writer(gcodeCfg);
    if (!writer.write(strokes, outputGcode)) return false;

    if (config.generatePreview && !config.previewPath.empty()) {
        generatePreview(simplified, wMm, hMm, config.previewPath);
    }

    return true;
}

bool MotionCompiler::compileCode(const std::string&,
    const std::string&) const {
    return false;
}
