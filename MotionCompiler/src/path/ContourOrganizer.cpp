#include "ContourOrganizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

double ContourOrganizer::distance(const Point& a, const Point& b) {
	const double dx = static_cast<double>(a.x - b.x);
	const double dy = static_cast<double>(a.y - b.y);
	return std::sqrt(dx * dx + dy * dy);
}

void ContourOrganizer::reverseContour(Contour& contour) {
	std::reverse(contour.points.begin(), contour.points.end());
}

Point ContourOrganizer::firstPoint(const Contour& contour) {
	return contour.points.front();
}

Point ContourOrganizer::lastPoint(const Contour& contour) {
	return contour.points.back();
}

ImageProfile ContourOrganizer::organize(const ImageProfile& profile) const {
	ImageProfile result;

	if (profile.contours.empty()) {
		return result;
	}

	std::vector<Contour> remaining = profile.contours;

	result.contours.reserve(remaining.size());

	// Начинаем с первого непустого контура
	// Позже можно сюда добавить
	// сортировку по площади
	// spatial index
	// иерархию outer/inner;
	// оптимизацию порядку контуров
	while (!remaining.empty()) {
		if (result.contours.empty()) {
			auto it = std::find_if(remaining.begin(), remaining.end(), [](const Contour& c) {
				return !c.points.empty();
			});

			if (it == remaining.end()) {
				break;
			}

			result.contours.push_back(*it);
			remaining.erase(it);
			continue;
		}

		const Point current = lastPoint(result.contours.back());

		size_t bestIndex = std::numeric_limits<size_t>::max();

		double bestDistance = std::numeric_limits<double>::max();

		bool bestReverse = false;

		for (size_t i = 0; i < remaining.size(); ++i) {
			const Contour& candidate = remaining[i];

			if (candidate.points.empty()) {
				continue;
			}

			const Point first = firstPoint(candidate);
			const Point last = lastPoint(candidate);

			const double dFirst = distance(current, first);
			const double dLast = distance(current, last);

			if (dFirst < bestDistance) {
				bestDistance = dFirst;
				bestIndex = i;
				bestReverse = false;
			}
			if (dLast < bestDistance) {
				bestDistance = dLast;
				bestIndex = i;
				bestReverse = true;
			}
		}

		if (bestIndex == std::numeric_limits<size_t>::max()) {
			break;
		}

		Contour selected = remaining[bestIndex];

		if (bestReverse) {
			reverseContour(selected);
		}

		result.contours.push_back(std::move(selected));

		remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(bestIndex));
	}

	return result;
}
