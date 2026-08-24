#include "MotionCompiler.h"

#include "../gcode/GCodeWriter.h"
#include "../image/ImageProcessor.h"
#include "../path/ContourOrganizer.h"

#include <fstream>

MotionCompiler::MotionCompiler() = default;

MotionCompiler::~MotionCompiler() = default;

bool MotionCompiler::compileImageProfiles(std::string inputFilename, std::string outputFilename, bool useSkeleton) {
	ImageProcessor imageProcessor;

	cv::Mat image;

	if (!imageProcessor.loadImage(inputFilename, image)) {
		return false;
	}

	cv::Mat binary = imageProcessor.toBinary(image);

	if (binary.empty()) {
		return false;
	}

	ImageProfile profile = imageProcessor.extractProfile(binary, useSkeleton);

	if (profile.contours.empty()) {
		return false;
	}

	ContourOrganizer organizer;

	ImageProfile organized = organizer.organize(profile);

	if (organized.contours.empty()) {
		return false;
	}

	GCodeConfig config;
	
	// пока тестовые значения, позже они станут частью настроек компилятора
	config.travelSpeedMmMin = 2000.0;
	config.printSpeedMmMin = 500.0;
	config.liftHeightMm = 5.0;
	// это не принтерная калибровка а просто начальная визуальная конверсия
	config.pixelsPerMm = 5.0;

	GCodeWriter writer(config);

	return writer.write(organized, outputFilename);
}

bool MotionCompiler::compileCode(std::string inputFilename, std::string outputFilename) {

	(void)inputFilename;
	(void)outputFilename;

	return false;
}
