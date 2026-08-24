#pragma once

#include "../model/ImageProfile.h"

#include <fstream>
#include <string>

struct GCodeConfig {
	double travelSpeedMmMin = 2000.0;
	double printSpeedMmMin = 500.0;

	double liftHeightMm = 5.0;
	double startX = 0.0;
	double startY = 0.0;

	// Пока используем простое масштабирование pixel -> mm
	// Позже это заменится на нормальную calibration/transform матрицу
	double pixelsPerMm = 5.0;
	bool returnToOrigin = true;
};

class GCodeWriter {
public:
	explicit GCodeWriter(GCodeConfig writerConfig = {});

	bool write(const ImageProfile& profile, const std::string& filename) const;

private:
	void writeHeader(std::ofstream& out) const;

	void writeFooter(std::ofstream& out) const;

	void writeTravel(std::ofstream& out, const Point& target) const;

	void writeMove(std::ofstream& out, const Point& target) const;

	void writePenUp(std::ofstream& out) const;

	void writePenDown(std::ofstream& out) const;

	double toMm(int pixel) const;

	GCodeConfig config;
};
