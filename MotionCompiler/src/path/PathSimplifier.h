#pragma once

#include "../model/ImageProfile.h"

#include <vector>

struct SimplificationConfig {
    bool enabled = true;

    // RDP
    double rdpEpsilon = 2.0;

    // Дополнительное удаление точек,
    // почти лежащих на прямой.
    double deviationThreshold = 3.0;

    // Сглаживание скользящим средним.
    // 0 = отключено.
    int smoothingWindow = 0;
    int smoothingPasses = 0;
};

class PathSimplifier {
public:
    explicit PathSimplifier(SimplificationConfig config = {});

    Contour simplify(const Contour& contour) const;
    ImageProfile simplify(const ImageProfile& profile) const;

private:
    SimplificationConfig config;

    static double pointToSegmentDistance(
        const Point& p,
        const Point& a,
        const Point& b);

    static void rdp(
        const std::vector<Point>& input,
        double epsilon,
        std::vector<Point>& output);

    static std::vector<Point> byDeviation(
        const std::vector<Point>& input,
        double maxDeviation);

    static std::vector<Point> smooth(
        const std::vector<Point>& input,
        int window,
        int passes);
};
