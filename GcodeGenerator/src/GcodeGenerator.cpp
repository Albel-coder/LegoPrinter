#define GCODEGENERATOR_EXPORTS
#include "GcodeGenerator.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <map>
#include <ctype.h>
#include <filesystem>

#include "FontManager.h"
#include "GenerateGcode.h"

struct ContourInfo
{
    std::vector<cv::Point> points;
    cv::Rect boundingRect;
    double area = 0.0;
    double perimeter = 0.0;

    ContourInfo() : area(0.0), perimeter(0.0) {}
};

class SimpleContourDetector
{
public:
    std::vector<ContourInfo> detectAllContours(const std::string& imagePath)
    {
        // Uploading an image
        cv::Mat image = cv::imread(imagePath);
        if (image.empty())
        {
            //std::cout << "Error: Could not load image " << imagePath << "\n";
            return {};
        }

        //std::cout << "Image: " << image.cols << "x" << image.rows << "\n";

        // Simple preprocessing
        cv::Mat gray, binary;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

        // Binarization is a simple method
        cv::threshold(gray, binary, 128, 255, cv::THRESH_BINARY);

        // Search ALL contours
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(binary, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        //std::cout << "Found " << contours.size() << " raw contours\n";

        // Transformation into our structure
        std::vector<ContourInfo> result;
        for (size_t i = 0; i < contours.size(); i++)
        {
            if (contours[i].size() < 3)
            {
                continue; // Minimum 3 points
            }

            ContourInfo info;
            info.points = contours[i];
            info.boundingRect = cv::boundingRect(contours[i]);
            info.area = cv::contourArea(contours[i]);
            info.perimeter = cv::arcLength(contours[i], true);

            // Basic filter - remove too small contours
            if (info.area > 5.0)
            { // Minimum area 5 pixels
                result.push_back(info);
            }
        }

        //std::cout << "After basic filtering: " << result.size() << " contours\n";

        // Visualization
        visualizeContours(image, result, imagePath);

        return result;
    }

private:
    void visualizeContours(const cv::Mat& original,
        const std::vector<ContourInfo>& contours,
        const std::string& imagePath) {
        cv::Mat result = original.clone();

        // We draw all the contours
        for (const auto& contour : contours)
        {
            // Random color for each outline
            cv::Scalar color(rand() % 256, rand() % 256, rand() % 256);

            // Drawing a bounding box
            cv::rectangle(result, contour.boundingRect, color, 2);

            // Draw the outline itself
            std::vector<std::vector<cv::Point>> contoursToDraw = { contour.points };
            cv::drawContours(result, contoursToDraw, -1, color, 1);
        }

        // Adding information
        std::string info = "Contours: " + std::to_string(contours.size());
        cv::putText(result, info, cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);

        // Save the result
        size_t lastDot = imagePath.find_last_of(".");
        std::string resultPath = imagePath.substr(0, lastDot) + "_all_contours.jpg";
        cv::imwrite(resultPath, result);

        //std::cout << "Visualization saved: " << resultPath << "\n";
    }
};

// Function to obtain the skeleton of a binary image
cv::Mat skeletonize(const cv::Mat& binary) {
    cv::Mat skel(binary.size(), CV_8UC1, cv::Scalar(0));
    cv::Mat temp, eroded;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));

    bool done;
    do {
        cv::morphologyEx(binary, temp, cv::MORPH_OPEN, element);
        cv::bitwise_not(temp, temp);
        cv::bitwise_and(binary, temp, temp);
        cv::bitwise_or(skel, temp, skel);
        cv::erode(binary, binary, element);

        double max;
        cv::minMaxLoc(binary, 0, &max);
        done = (max == 0);
    } while (!done);

    return skel;
}

// Function for obtaining thin outlines of characters
std::vector<std::vector<cv::Point>> getThinContours(const cv::Mat& image, int penWidth) {
    // 1. Image binarization
    cv::Mat gray, binary;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = image.clone();
    }
    cv::threshold(gray, binary, 127, 255, cv::THRESH_BINARY_INV);

    // 2. Morphological closure for connecting close contours
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(penWidth / 2, penWidth / 2));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);

    // 3. Getting the skeleton
    cv::Mat skeleton = skeletonize(binary);

    // 4. Search for skeletal contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skeleton, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    return contours;
}

// An alternative approach: homothety (contour scaling)
std::vector<std::vector<cv::Point>> getScaledContours(const cv::Mat& image, int penWidth) {
    // 1. Binarization
    cv::Mat gray, binary;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = image.clone();
    }
    cv::threshold(gray, binary, 127, 255, cv::THRESH_BINARY_INV);

    // 2. Finding outer contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 3. Scaling contours inward
    std::vector<std::vector<cv::Point>> scaledContours;
    cv::Point2f center;
    float radius;

    for (auto& contour : contours) {
        // Finding the minimum bounding circle
        cv::minEnclosingCircle(contour, center, radius);

        // Calculating the scaling factor
        double scale = 1.0 - (static_cast<double>(penWidth) / (radius * 2));
        scale = std::max(scale, 0.1); // Limiting the minimum scale

        // Scaling the outline
        std::vector<cv::Point> scaledContour;
        for (auto& point : contour) {
            cv::Point2f relative(point.x - center.x, point.y - center.y);
            relative *= scale;
            scaledContour.push_back(cv::Point(
                static_cast<int>(relative.x + center.x),
                static_cast<int>(relative.y + center.y)
            ));
        }
        scaledContours.push_back(scaledContour);
    }

    return scaledContours;
}

void saveContoursToFiles(const std::vector<ContourInfo>& contours, const std::string& baseName)
{
    std::string simpleFileName = "simple_contours_" + baseName + ".txt";
    std::string detailedFileName = "all_contours_data_" + baseName + ".txt";

    std::ofstream simpleFile(simpleFileName);
    std::ofstream detailedFile(detailedFileName);

    if (simpleFile.is_open() && detailedFile.is_open())
    {

        for (size_t i = 0; i < contours.size(); i++)
        {
            const auto& contour = contours[i];

            // Detailed information
            detailedFile << "CONTOUR " << i << ":\n";
            for (size_t j = 0; j < contour.points.size(); j++)
            {
                detailedFile << contour.points[j].x << " " << contour.points[j].y << "\n";
            }
        }

        simpleFile.close();
        detailedFile.close();
        //std::cout << "Contour data saved to " << simpleFileName << " and " << detailedFileName << "\n";
    }
    else
    {
        //std::cout << "Error: Could not open contour files for writing: " << baseName << "\n";
    }
}

struct ContourGroup
{
    std::vector<cv::Point> otherContour;
    std::vector<std::vector<cv::Point>> innerContours;
    cv::Rect boundingRect;
};

class AdvancedContourDetector
{
public:
    std::vector<ContourGroup> extractContourGroups(const std::string& imagePath)
    {
        cv::Mat image = cv::imread(imagePath, cv::IMREAD_GRAYSCALE);
        if (image.empty())
        {
            //std::cout << "Error: could not load image " << ImagePath << "\n";
            return {};
        }

        // Binarization with adaptive thresholding for better quality
        cv::Mat binary;
        cv::threshold(image, binary, 128, 255, cv::THRESH_BINARY_INV);

        // Finding contours with hierarchy to determine nesting
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(binary, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_TC89_KCOS);

        return groupContoursByTopology(contours, hierarchy);
    }

    void saveContoursGroupToFile(const std::vector<ContourGroup>& groups, const std::string& filename)
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            //std::cout << "Error: could not open file " << Filename << " for writing\n";
            return;
        }
        else
        {
            for (size_t groupIndex = 0; groupIndex < groups.size(); groupIndex++)
            {
                const auto& group = groups[groupIndex];

                // External contour
                file << "OUTER_CONTOUR " << groupIndex << ":\n";
                for (const auto& point : group.otherContour)
                {
                    file << point.x << " " << point.y << "\n";
                }

                // Internal contours
                for (size_t innerIndex = 0; innerIndex < group.innerContours.size(); innerIndex++)
                {
                    file << "INNER_CONTOUR " << groupIndex << "_" << innerIndex << ":\n";
                    for (const auto& point : group.innerContours[innerIndex])
                    {
                        file << point.x << " " << point.y << "\n";
                    }
                }
            }
        }

        file.close();
        //std::cout << "Contour group saved to: " << Filename << "\n";
    }

private:
    std::vector<ContourGroup> groupContoursByTopology(const std::vector<std::vector<cv::Point>> contours,
        const std::vector<cv::Vec4i>& hierarchy)
    {
        std::vector<ContourGroup> groups;
        std::vector<bool> processed(contours.size(), false);

        for (size_t i = 0; i < contours.size(); i++)
        {
            if (processed[i] || hierarchy[i][3] != -1) // We skip already processed and internal contours
            {
                continue;
            }
            else
            {
                ContourGroup group;
                group.otherContour = contours[i];
                group.boundingRect = cv::boundingRect(contours[i]);
                processed[i] = true;

                // We are looking for internal contours for this external contour
                int childIndex = hierarchy[i][2]; // First child circuit
                while (childIndex != -1)
                {
                    if (!processed[childIndex])
                    {
                        group.innerContours.push_back(contours[childIndex]);
                        processed[childIndex] = true;
                    }

                    childIndex = hierarchy[childIndex][0]; // The next contour is at the same level
                }

                // Filter by area - remove too small contours
                double Area = cv::contourArea(group.otherContour);
                if (Area > 10.0) // Minimum area - 10 pixels
                {
                    groups.push_back(group);
                }
            }

            return groups;
        }
    }
};

void saveAdvancedContours(const std::vector<ContourGroup>& contourGroups, const std::string& baseName)
{
    AdvancedContourDetector detector;
    std::string filename = "advanced_contours_" + baseName + ".txt";
    detector.saveContoursGroupToFile(contourGroups, filename);
}





class Generator {
public:
	bool generateImageProfiles(const char* inputFilename, const char* outputFilename) {

        SimpleContourDetector contourDetector;
        auto contours = contourDetector.detectAllContours(inputFilename);

        // Saving contour information to files for the test image
        saveContoursToFiles(contours, outputFilename);
		return true;
	}

    bool generateCode(const char* inputFilename, const char* outputFilename) {
        generateTestGcode(inputFilename, outputFilename);
		return true;
    }
};

extern "C"
{
	GCODE_GENERATOR_API GcodeGeneratorHandle CreateGenerator() {
		return new Generator();
	}

	GCODE_GENERATOR_API void DestroyGenerator(GcodeGeneratorHandle handle) {
		delete static_cast<Generator*>(handle);
	}

	GCODE_GENERATOR_API bool GenerateImageProfiles(GcodeGeneratorHandle handle, const char* inputFilename, const char* outputFilename) {
		if (!handle) return false;

		return static_cast<Generator*>(handle)->generateImageProfiles(inputFilename, outputFilename);
	}

    GCODE_GENERATOR_API bool GenerateCode(GcodeGeneratorHandle handle, const char* inputFilename, const char* outputFilename) {
        if (!handle) return false;

        return static_cast<Generator*>(handle)->generateCode(inputFilename, outputFilename);
    }
}
