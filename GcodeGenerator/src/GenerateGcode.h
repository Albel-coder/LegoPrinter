#pragma once
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

bool GenerateTestGcode(const std::string& Filename);

class SimpleGcodeGenerator
{
public:
	bool GenerateCode(const std::string& ContourFilename, const std::string& OutputFilename);
	void SetPrintingParameters(double TravelSpeed, double PrintSpeed, double ZHopHeight);

private:
	double TravelSpeed = 40.0;
	double PrintSpeed = 20.0;
	double ZHopHeight = 5.0;

	std::string GenerateGcodeForContour(const std::vector<cv::Point>& Contour, bool IsOther);
	double CalculateDistance(const cv::Point& FirstPoint, const cv::Point& SecondPoint);
	cv::Point FindNearestPoint(const cv::Point& Reference, const std::vector<cv::Point>& Points);
};