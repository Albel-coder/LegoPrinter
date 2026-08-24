#pragma once

#include "Point.h"

#include <vector>

struct Contour {
	std::vector<Point> points;
	bool closed = true;
};
