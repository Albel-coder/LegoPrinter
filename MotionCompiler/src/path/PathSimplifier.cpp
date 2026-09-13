#include "PathSimplifier.h"

#include <algorithm>
#include <cmath>

namespace {

    double pointToSegmentDistanceImpl(
        const Point& p,
        const Point& a,
        const Point& b)
    {
        const double dx = static_cast<double>(b.x - a.x);
        const double dy = static_cast<double>(b.y - a.y);
        const double len2 = dx * dx + dy * dy;

        if (len2 < 1e-12) {
            const double ex = static_cast<double>(p.x - a.x);
            const double ey = static_cast<double>(p.y - a.y);
            return std::sqrt(ex * ex + ey * ey);
        }

        const double px = static_cast<double>(p.x - a.x);
        const double py = static_cast<double>(p.y - a.y);

        double t = (px * dx + py * dy) / len2;
        t = std::max(0.0, std::min(1.0, t));

        const double cx = a.x + t * dx;
        const double cy = a.y + t * dy;

        const double ex = static_cast<double>(p.x) - cx;
        const double ey = static_cast<double>(p.y) - cy;

        return std::sqrt(ex * ex + ey * ey);
    }

} // namespace

PathSimplifier::PathSimplifier(SimplificationConfig config)
    : config(config)
{
}

double PathSimplifier::pointToSegmentDistance(
    const Point& p,
    const Point& a,
    const Point& b)
{
    return pointToSegmentDistanceImpl(p, a, b);
}

void PathSimplifier::rdp(
    const std::vector<Point>& input,
    double epsilon,
    std::vector<Point>& output)
{
    if (input.size() < 2) {
        output = input;
        return;
    }

    double maxDistance = 0.0;
    size_t maxIndex = 0;

    const Point& first = input.front();
    const Point& last = input.back();

    for (size_t i = 1; i + 1 < input.size(); ++i) {
        const double d = pointToSegmentDistanceImpl(
            input[i], first, last);

        if (d > maxDistance) {
            maxDistance = d;
            maxIndex = i;
        }
    }

    if (maxDistance > epsilon) {

        std::vector<Point> left(
            input.begin(),
            input.begin() + static_cast<std::ptrdiff_t>(maxIndex) + 1);

        std::vector<Point> right(
            input.begin() + static_cast<std::ptrdiff_t>(maxIndex),
            input.end());

        std::vector<Point> leftResult;
        std::vector<Point> rightResult;

        rdp(left, epsilon, leftResult);
        rdp(right, epsilon, rightResult);

        output = leftResult;
        output.insert(
            output.end(),
            rightResult.begin() + 1,
            rightResult.end());
    }
    else {
        output.clear();
        output.push_back(first);
        output.push_back(last);
    }
}

std::vector<Point> PathSimplifier::byDeviation(
    const std::vector<Point>& input,
    double maxDeviation)
{
    if (input.size() < 3) {
        return input;
    }

    std::vector<Point> output;
    output.reserve(input.size());

    size_t anchor = 0;
    output.push_back(input[anchor]);

    while (anchor + 1 < input.size()) {
        size_t best = anchor + 1;

        for (size_t candidate = anchor + 2;
            candidate < input.size();
            ++candidate) {

            bool valid = true;

            for (size_t j = anchor + 1; j < candidate; ++j) {
                const double deviation = pointToSegmentDistanceImpl(
                    input[j], input[anchor], input[candidate]);

                if (deviation > maxDeviation) {
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                break;
            }

            best = candidate;
        }

        output.push_back(input[best]);
        anchor = best;
    }

    return output;
}

std::vector<Point> PathSimplifier::smooth(
    const std::vector<Point>& input,
    int window,
    int passes)
{
    if (input.size() < 3 || window <= 0 || passes <= 0) {
        return input;
    }

    std::vector<Point> current = input;

    for (int pass = 0; pass < passes; ++pass) {

        std::vector<Point> next = current;

        for (size_t i = 1; i + 1 < current.size(); ++i) {

            double sumX = 0.0;
            double sumY = 0.0;
            int count = 0;

            for (int j = -window; j <= window; ++j) {

                int idx = static_cast<int>(i) + j;

                if (idx < 0) idx = 0;
                if (idx >= static_cast<int>(current.size()))
                    idx = static_cast<int>(current.size()) - 1;

                sumX += current[idx].x;
                sumY += current[idx].y;
                ++count;
            }

            next[i].x = static_cast<int>(std::lround(sumX / count));
            next[i].y = static_cast<int>(std::lround(sumY / count));
        }

        current = std::move(next);
    }

    return current;
}

Contour PathSimplifier::simplify(const Contour& contour) const
{
    if (!config.enabled || contour.points.size() < 3) {
        return contour;
    }

    Contour result;

    std::vector<Point> working = contour.points;

    // 1. Smoothing (только если задано)
    if (config.smoothingWindow > 0 && config.smoothingPasses > 0) {
        working = smooth(
            working,
            config.smoothingWindow,
            config.smoothingPasses);
    }

    // 2. RDP
    std::vector<Point> afterRdp;

    if (config.rdpEpsilon > 0.0) {
        rdp(working, config.rdpEpsilon, afterRdp);
    }
    else {
        afterRdp = working;
    }

    // 3. Deviation simplification
    std::vector<Point> finalPoints;

    if (config.deviationThreshold > 0.0) {
        finalPoints = byDeviation(afterRdp, config.deviationThreshold);
    }
    else {
        finalPoints = afterRdp;
    }

    result.points = std::move(finalPoints);
    result.closed = contour.closed;

    return result;
}

ImageProfile PathSimplifier::simplify(const ImageProfile& profile) const
{
    ImageProfile result;

    if (!config.enabled) {
        return profile;
    }

    result.contours.reserve(profile.contours.size());

    for (const auto& contour : profile.contours) {
        result.contours.push_back(simplify(contour));
    }

    return result;
}
