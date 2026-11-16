#include "ContourOrganizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace {

    struct Bounds {
        double minX, maxX, minY, maxY;
    };

    Bounds computeBounds(const std::vector<Point>& pts) {
        Bounds b{ pts[0].x, pts[0].x, pts[0].y, pts[0].y };
        for (const auto& p : pts) {
            if (p.x < b.minX) b.minX = p.x;
            if (p.x > b.maxX) b.maxX = p.x;
            if (p.y < b.minY) b.minY = p.y;
            if (p.y > b.maxY) b.maxY = p.y;
        }
        return b;
    }

} // namespace

ContourOrganizer::ContourOrganizer(Config config) : config(config) {}

std::vector<size_t> ContourOrganizer::readingOrder(const ImageProfile& profile,
    bool flipY) const {
    const size_t n = profile.contours.size();

    std::unordered_map<size_t, std::vector<size_t>> children;
    for (size_t j = 0; j < n; ++j) {
        const int parent = profile.contours[j].parent;
        if (parent >= 0) {
            children[static_cast<size_t>(parent)].push_back(j);
        }
    }

    struct RowInfo { double cy, h, x; };
    std::unordered_map<size_t, RowInfo> info;
    std::vector<size_t> outers;

    const double sign = flipY ? -1.0 : 1.0;

    for (size_t i = 0; i < n; ++i) {
        if (profile.contours[i].parent >= 0) continue;
        const auto& pts = profile.contours[i].sourcePoints.empty()
            ? profile.contours[i].points
            : profile.contours[i].sourcePoints;
        if (pts.empty()) continue;
        const Bounds b = computeBounds(pts);
        info[i] = { sign * (b.minY + b.maxY) / 2.0, b.maxY - b.minY, b.minX };
        outers.push_back(i);
    }

    std::sort(outers.begin(), outers.end(), [&](size_t a, size_t b) {
        return info[a].cy < info[b].cy;
        });

    std::vector<std::vector<size_t>> rows;
    for (size_t i : outers) {
        bool placed = false;
        for (auto& row : rows) {
            const auto& ri = info[row[0]];
            if (std::abs(ri.cy - info[i].cy) < 0.6 * std::max(info[i].h, ri.h)) {
                row.push_back(i);
                placed = true;
                break;
            }
        }
        if (!placed) rows.push_back({ i });
    }

    std::vector<size_t> order;
    for (auto& row : rows) {
        std::sort(row.begin(), row.end(), [&](size_t a, size_t b) {
            return info[a].x < info[b].x;
            });
        for (size_t i : row) {
            order.push_back(i);

            auto it = children.find(i);
            if (it == children.end()) continue;

            auto kids = it->second;
            std::sort(kids.begin(), kids.end(), [&](size_t a, size_t b) {
                return profile.contours[a].area > profile.contours[b].area;
                });
            order.insert(order.end(), kids.begin(), kids.end());
        }
    }

    return order;
}

ImageProfile ContourOrganizer::planStrokes(const ImageProfile& profile, bool flipY) const {
    ImageProfile result;
    if (profile.contours.empty()) return result;

    std::vector<size_t> remaining = readingOrder(profile, flipY);
    result.contours.reserve(remaining.size());

    Point pos{ 0.0, 0.0 };

    while (!remaining.empty()) {
        size_t pickIdx = 0;

        if (config.order == OrderMode::Nearest) {
            double bestD = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < remaining.size(); ++k) {
                const auto& ring = profile.contours[remaining[k]].points;
                for (size_t j = 0; j < ring.size(); j += 4) {
                    const double dx = ring[j].x - pos.x;
                    const double dy = ring[j].y - pos.y;
                    const double d = dx * dx + dy * dy;
                    if (d < bestD) {
                        bestD = d;
                        pickIdx = k;
                    }
                }
            }
        }

        const size_t srcIdx = remaining[pickIdx];
        remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(pickIdx));

        const auto& ring = profile.contours[srcIdx].points;
        if (ring.empty()) continue;

        size_t startIdx = 0;
        double bestStart = std::numeric_limits<double>::infinity();
        for (size_t j = 0; j < ring.size(); ++j) {
            const double dx = ring[j].x - pos.x;
            const double dy = ring[j].y - pos.y;
            const double d = dx * dx + dy * dy;
            if (d < bestStart) {
                bestStart = d;
                startIdx = j;
            }
        }

        Contour stroke;
        stroke.closed = true;
        stroke.points.reserve(ring.size() + 1);
        for (size_t j = 0; j < ring.size(); ++j) {
            stroke.points.push_back(ring[(startIdx + j) % ring.size()]);
        }
        stroke.points.push_back(stroke.points.front());

        pos = stroke.points.back();
        result.contours.push_back(std::move(stroke));
    }

    return result;
}
