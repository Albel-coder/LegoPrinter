#pragma once

#include "Point.h"
#include <vector>

struct Contour {
    // Итоговые точки для рисования (мм, после RDP).
    // Для замкнутых контуров замыкающая точка НЕ дублируется здесь —
    // это делает ContourOrganizer::planStrokes.
    std::vector<Point> points;

    // Сырые точки сразу после extractContours (до RDP), мм.
    // Нужны для чтения-порядка (bounds) и для расчета area.
    std::vector<Point> sourcePoints;

    // Площадь полигона по sourcePoints, мм^2.
    double area = 0.0;

    // Индекс родителя в ImageProfile или -1 для внешнего контура.
    int parent = -1;

    bool closed = true;
};
