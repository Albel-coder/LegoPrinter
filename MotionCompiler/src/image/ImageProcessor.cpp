#include "ImageProcessor.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

bool ImageProcessor::loadImage(const std::string& filename, cv::Mat& image) const {
	image = cv::imread(filename, cv::IMREAD_COLOR);

	return !image.empty();
}

cv::Mat ImageProcessor::toBinary(const cv::Mat& image) const {
	cv::Mat gray;
	cv::Mat binary;

	if (image.empty()) {
		return {};
	}

	if (image.channels() == 1) {
		gray = image.clone();
	}
	else {
		cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
	}

	// Белый объект на черном фоне
	// Позже threshold можно вынести в конфигурацию compiler
	cv::threshold(gray, binary, 128, 255, cv::THRESH_BINARY_INV);

	return binary;
}

cv::Mat ImageProcessor::skeletonize(const cv::Mat& binary) const {
	if (binary.empty()) {
		return {};
	}

	cv::Mat img;
	
	// Zhang-Suen предполагает бинарное изображение 0 / 255
	cv::threshold(binary, img, 127, 255, cv::THRESH_BINARY);

	bool changed = true;

	const int rows = img.rows;
	const int cols = img.cols;

	while (changed) {
		changed = false;

		std::vector<cv::Point> toDelete;
		toDelete.reserve(static_cast<size_t>((rows * cols) / 20));

		// Step 1
		for (int y = 1; y < rows - 1; ++y) {
			for (int x = 1; x < cols - 1; ++x) {
				if (img.at<uchar>(y, x) != 255) {
					continue;
				}

				const int p2 = img.at<uchar>(y - 1, x) > 0;
				const int p3 = img.at<uchar>(y - 1, x + 1) > 0;
				const int p4 = img.at<uchar>(y, x + 1) > 0;
				const int p5 = img.at<uchar>(y + 1, x + 1) > 0;
				const int p6 = img.at<uchar>(y + 1, x) > 0;
				const int p7 = img.at<uchar>(y + 1, x - 1) > 0;
				const int p8 = img.at<uchar>(y, x - 1) > 0;
				const int p9 = img.at<uchar>(y - 1, x - 1) > 0;

				const int neighbors = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;

				if (neighbors < 2 || neighbors > 6) {
					continue;
				}

				const int transitions =
					(!p2 && p3) +
					(!p3 && p4) +
					(!p4 && p5) +
					(!p5 && p6) +
					(!p6 && p7) +
					(!p7 && p8) +
					(!p8 && p9) +
					(!p9 && p2);

				if (transitions != 1) {
					continue;
				}

				if (p2 && p4 && p6) {
					continue;
				}

				if (p4 && p6 && p8) {
					continue;
				}

				toDelete.emplace_back(x, y);
			}
		}

		for (const auto& p : toDelete) {
			img.at<uchar>(p.y, p.x) = 0;
		}

		if (!toDelete.empty()) {
			changed = true;
		}

		// Step 2
		toDelete.clear();

		for (int y = 1; y < rows - 1; ++y) {
			for (int x = 1; x < cols - 1; ++x) {
				if (img.at<uchar>(y, x) != 255) {
					continue;
				}

				const int p2 = img.at<uchar>(y - 1, x) > 0;
				const int p3 = img.at<uchar>(y - 1, x + 1) > 0;
				const int p4 = img.at<uchar>(y, x + 1) > 0;
				const int p5 = img.at<uchar>(y + 1, x + 1) > 0;
				const int p6 = img.at<uchar>(y + 1, x) > 0;
				const int p7 = img.at<uchar>(y + 1, x - 1) > 0;
				const int p8 = img.at<uchar>(y, x - 1) > 0;
				const int p9 = img.at<uchar>(y - 1, x - 1) > 0;

				const int neighbors = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;

				if (neighbors < 2 || neighbors > 6) {
					continue;
				}

				const int transitions =
					(!p2 && p3) +
					(!p3 && p4) +
					(!p4 && p5) +
					(!p5 && p6) +
					(!p6 && p7) +
					(!p7 && p8) +
					(!p8 && p9) +
					(!p9 && p2);

				if (transitions != 1) {
					continue;
				}

				if (p2 && p4 && p8) {
					continue;
				}

				if (p2 && p6 && p8) {
					continue;
				}

				toDelete.emplace_back(x, y);
			}
		}

		for (const auto& p : toDelete) {
			img.at<uchar>(p.y, p.x) = 0;
		}

		if (!toDelete.empty()) {
			changed = true;
		}
	}

	return img;
}

Contour ImageProcessor::convertContour(const std::vector<cv::Point>& contour) {
	Contour result;

	result.points.reserve(contour.size());

	for (const auto& p : contour) {
		result.points.push_back({ p.x, p.y });
	}

	// findContours возвращает замкнутую границу для обычных контуров
	result.closed = true;

	return result;
}

std::vector<Contour> ImageProcessor::extractContours(const cv::Mat& binary, bool useSkeleton) const {
	std::vector<std::vector<cv::Point>> cvContours;

	const int retrievalMode = useSkeleton ? cv::RETR_LIST : cv::RETR_CCOMP;

	const int approximationMode = useSkeleton ? cv::CHAIN_APPROX_NONE : cv::CHAIN_APPROX_TC89_KCOS;

	cv::Mat working = binary.clone();

	cv::findContours(working, cvContours, retrievalMode, approximationMode);

	std::vector<Contour> result;
	result.reserve(cvContours.size());

	for (const auto& contour : cvContours) {
		if (contour.size() < 2) {
			continue;
		}

		const double perimeter = cv::arcLength(contour, true);

		const double area = std::abs(cv::contourArea(contour));

		// фильтр намеренно очень мягкий
		// сильную фильтрацию будет добавлять только после тестов
		if (perimeter < 5.0 && area < 2.0) {
			continue;
		}

		result.push_back(convertContour(contour));
	}

	return result;
}

ImageProfile ImageProcessor::extractProfile(const cv::Mat& binary, bool useSkeleton) const {
	ImageProfile profile;

	cv::Mat processed = binary.clone();

	if (useSkeleton) {
		processed = skeletonize(processed);
	}

	profile.contours = extractContours(processed, useSkeleton);

	return profile;
}
