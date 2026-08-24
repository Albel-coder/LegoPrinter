#pragma once

#include "../model/ImageProfile.h"

class ContourOrganizer {
public:
	ContourOrganizer() = default;
	~ContourOrganizer() = default;

	ImageProfile organize(const ImageProfile& profile) const;

private:
	static double distance(const Point& a, const Point& b);

	static void reverseContour(Contour& contour);

	static Point firstPoint(const Contour& contour);

	static Point lastPoint(const Contour& contour);
};
