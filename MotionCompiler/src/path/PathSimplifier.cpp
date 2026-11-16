#include "PathSimplifier.h"

#include <cmath>

PathSimplifier::PathSimplifier(PathSimplificationConfig config) : config(config) {}

double PathSimplifier::pointLineDistance(const Point& p, const Point& a, const Point& b) {
    const double vx = b.x - a.x;
    const double vy = b.y - a.y;
    const double len = std::sqrt(vx * vx + vy * vy);

    if (len < 1e-12) {
        const double dx = p.x - a.x;
        const double dy = p.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    const double wx = p.x - a.x;
    const double wy = p.y - a.y;
    const double cross = vx * wy - vy * wx;
    return std::abs(cross) / len;
}

void PathSimplifier::rdp(const std::vector<Point>& input,
    double epsilon,
    std::vector<Point>& output) {
    if (input.size() < 3) {
        output = input;
        return;
    }

    const Point& start = input.front();
    const Point& end = input.back();

    double dmax = 0.0;
    size_t index = 0;
    for (size_t i = 1; i + 1 < input.size(); ++i) {
        const double d = pointLineDistance(input[i], start, end);
        if (d > dmax) {
            dmax = d;
            index = i;
        }
    }

    if (dmax > epsilon) {
        std::vector<Point> left(input.begin(),
            input.begin() + static_cast<std::ptrdiff_t>(index) + 1);
        std::vector<Point> right(input.begin() + static_cast<std::ptrdiff_t>(index),
            input.end());

        std::vector<Point> leftResult, rightResult;
        rdp(left, epsilon, leftResult);
        rdp(right, epsilon, rightResult);

        output = std::move(leftResult);
        output.insert(output.end(), rightResult.begin() + 1, rightResult.end());
    }
    else {
        output.clear();
        output.push_back(start);
        output.push_back(end);
    }
}

std::vector<Point> PathSimplifier::simplifyClosed(const std::vector<Point>& P,
    double epsilon) {
    if (P.size() < 4) return P;

    // Индекс точки, максимально удалённой от P[0]
    size_t splitIdx = 0;
    double maxDistSq = -1.0;
    for (size_t i = 0; i < P.size(); ++i) {
        const double dx = P[i].x - P[0].x;
        const double dy = P[i].y - P[0].y;
        const double d2 = dx * dx + dy * dy;
        if (d2 > maxDistSq) {
            maxDistSq = d2;
            splitIdx = i;
        }
    }

    std::vector<Point> half1(P.begin(),
        P.begin() + static_cast<std::ptrdiff_t>(splitIdx) + 1);
    std::vector<Point> half2(P.begin() + static_cast<std::ptrdiff_t>(splitIdx),
        P.end());
    half2.push_back(P[0]);

    std::vector<Point> s1, s2;
    rdp(half1, epsilon, s1);
    rdp(half2, epsilon, s2);

    std::vector<Point> result;
    result.reserve(s1.size() + s2.size());
    result.insert(result.end(), s1.begin(), s1.end() - 1);
    result.insert(result.end(), s2.begin(), s2.end() - 1);
    return result;
}

Contour PathSimplifier::simplify(const Contour& contour) const {
    Contour result = contour;
    const auto& input = contour.sourcePoints.empty()
        ? contour.points
        : contour.sourcePoints;
    result.points = simplifyClosed(input, config.rdpEpsilonMm);
    return result;
}

ImageProfile PathSimplifier::simplify(const ImageProfile& profile) const {
    ImageProfile result;
    result.contours.reserve(profile.contours.size());
    for (const auto& c : profile.contours) {
        result.contours.push_back(simplify(c));
    }
    return result;
}
