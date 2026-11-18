#pragma once

#include "../model/ImageProfile.h"

#include <vector>

struct PathSimplificationConfig {
    double rdpEpsilonMm = 0.50;
};

class PathSimplifier {
public:
    explicit PathSimplifier(PathSimplificationConfig config = {});

    Contour simplify(const Contour& contour) const;
    ImageProfile simplify(const ImageProfile& profile) const;

    // Public для тестов.
    static double pointLineDistance(const Point& p, const Point& a, const Point& b);

private:
    // Классический RDP по открытой полилинии.
    static void rdp(const std::vector<Point>& input,
        double epsilon,
        std::vector<Point>& output);

    // RDP для замкнутого контура: расщепляем в самой удалённой от P[0] точке.
    static std::vector<Point> simplifyClosed(const std::vector<Point>& points,
        double epsilon);

    PathSimplificationConfig config;
};
