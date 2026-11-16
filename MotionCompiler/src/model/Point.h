#pragma once

// Точка в координатах станка (мм). Все вычисления после ImageProcessor — в double.
struct Point {
    double x = 0.0;
    double y = 0.0;
};
