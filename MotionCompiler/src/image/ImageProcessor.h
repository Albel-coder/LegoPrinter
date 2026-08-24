#pragma once

#include "../model/ImageProfile.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

class ImageProcessor {
public:
	ImageProcessor() = default;
	~ImageProcessor() = default;

	bool loadImage(const std::string& filename, cv::Mat& image) const;

	cv::Mat toBinary(const cv::Mat& image) const;

	cv::Mat skeletonize(const cv::Mat& binary) const;

	ImageProfile extractProfile(const cv::Mat& binary, bool useSkeleton) const;

private:
	std::vector<Contour> extractContours(const cv::Mat& binary, bool useSkeleton) const;

	static Contour convertContour(const std::vector<cv::Point>& contour);
};
