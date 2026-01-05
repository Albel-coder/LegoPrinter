#pragma once
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

bool generateTestGcode(const std::string& filename);

class SimpleGcodeGenerator
{
public:
	bool generateCode(const std::string& contourFilename, const std::string& outputFilename);
	void setPrintingParameters(double travelSpeed, double printSpeed, double zHopHeight);

private:
	double travelSpeed = 40.0;
	double printSpeed = 20.0;
	double zHopHeight = 5.0;

	std::string generateGcodeForContour(const std::vector<cv::Point>& contour, bool isOther);
	double calculateDistance(const cv::Point& firstPoint, const cv::Point& secondPoint);
	cv::Point findNearestPoint(const cv::Point& reference, const std::vector<cv::Point>& points);
};