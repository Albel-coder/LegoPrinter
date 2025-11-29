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
            std::cout << "Error: Could not load image " << imagePath << "\n";
            return {};
        }

        std::cout << "Image: " << image.cols << "x" << image.rows << "\n";

        // Simple preprocessing
        cv::Mat gray, binary;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

        // Binarization is a simple method
        cv::threshold(gray, binary, 128, 255, cv::THRESH_BINARY);

        // Search ALL contours
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(binary, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        std::cout << "Found " << contours.size() << " raw contours\n";

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

        std::cout << "After basic filtering: " << result.size() << " contours\n";

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

        std::cout << "Visualization saved: " << resultPath << "\n";
    }
};

// Function for create test png image with different characters
void createTestShapes(const std::string& outputPath)
{
    cv::Mat image(600, 800, CV_8UC3, cv::Scalar(255, 255, 255));

    // Let's draw different figures
    cv::rectangle(image, cv::Point(50, 50), cv::Point(200, 200), cv::Scalar(0, 0, 0), 2);
    cv::circle(image, cv::Point(300, 125), 75, cv::Scalar(0, 0, 0), 2);

    cv::ellipse(image, cv::Point(500, 125), cv::Size(100, 50), 0, 0, 360, cv::Scalar(0, 0, 0), 2);

    // Triangle
    cv::Point triangle[3] = { cv::Point(100, 300), cv::Point(50, 400), cv::Point(150, 400) };
    cv::fillConvexPoly(image, triangle, 3, cv::Scalar(0, 0, 0));

    // Polygon
    cv::Point polygon[6] =
    {
        cv::Point(250, 300), cv::Point(300, 250), cv::Point(350, 250),
        cv::Point(400, 300), cv::Point(350, 400), cv::Point(300, 400)
    };
    cv::polylines(image, std::vector<cv::Point>(polygon, polygon + 6), true, cv::Scalar(0, 0, 0), 2);

    // Text is also like a set of outlines
    cv::putText(image, "G-CODE TEST", cv::Point(450, 350),
        cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);

    cv::imwrite(outputPath, image);
    std::cout << "Test shapes image created: " << outputPath << "\n";
}

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

void SaveContoursToFiles(const std::vector<ContourInfo>& contours, const std::string& baseName)
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
        std::cout << "Contour data saved to " << simpleFileName << " and " << detailedFileName << "\n";
    }
    else
    {
        std::cout << "Error: Could not open contour files for writing: " << baseName << "\n";
    }
}

struct ContourGroup
{
    std::vector<cv::Point> OtherContour;
    std::vector<std::vector<cv::Point>> InnerContours;
    cv::Rect BoundingRect;
};

class AdvancedContourDetector
{
public:
    std::vector<ContourGroup> ExtractContourGroups(const std::string& ImagePath)
    {
        cv::Mat Image = cv::imread(ImagePath, cv::IMREAD_GRAYSCALE);
        if (Image.empty())
        {
            std::cout << "Error: could not load image " << ImagePath << "\n";
            return {};
        }

        // Binarization with adaptive thresholding for better quality
        cv::Mat Binary;
        cv::threshold(Image, Binary, 128, 255, cv::THRESH_BINARY_INV);

        // Finding contours with hierarchy to determine nesting
        std::vector<std::vector<cv::Point>> Contours;
        std::vector<cv::Vec4i> Hierarchy;
        cv::findContours(Binary, Contours, Hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_TC89_KCOS);

        return GroupContoursByTopology(Contours, Hierarchy);
    }

    void SaveContoursGroupToFile(const std::vector<ContourGroup>& Groups, const std::string& Filename)
    {
        std::ofstream File(Filename);
        if (!File.is_open())
        {
            std::cout << "Error: could not open file " << Filename << " for writing\n";
            return;
        }
        else
        {
            for (size_t GroupIndex = 0; GroupIndex < Groups.size(); GroupIndex++)
            {
                const auto& Group = Groups[GroupIndex];

                // External contour
                File << "OUTER_CONTOUR " << GroupIndex << ":\n";
                for (const auto& Point : Group.OtherContour)
                {
                    File << Point.x << " " << Point.y << "\n";
                }

                // Internal contours
                for (size_t InnerIndex = 0; InnerIndex < Group.InnerContours.size(); InnerIndex++)
                {
                    File << "INNER_CONTOUR " << GroupIndex << "_" << InnerIndex << ":\n";
                    for (const auto& Point : Group.InnerContours[InnerIndex])
                    {
                        File << Point.x << " " << Point.y << "\n";
                    }
                }
            }
        }

        File.close();
        std::cout << "Contour group saved to: " << Filename << "\n";
    }

private:
    std::vector<ContourGroup> GroupContoursByTopology(const std::vector<std::vector<cv::Point>> Contours,
        const std::vector<cv::Vec4i>& Hierarchy)
    {
        std::vector<ContourGroup> Groups;
        std::vector<bool> Processed(Contours.size(), false);

        for (size_t i = 0; i < Contours.size(); i++)
        {
            if (Processed[i] || Hierarchy[i][3] != -1) // We skip already processed and internal contours
            {
                continue;
            }
            else
            {
                ContourGroup Group;
                Group.OtherContour = Contours[i];
                Group.BoundingRect = cv::boundingRect(Contours[i]);
                Processed[i] = true;

                // We are looking for internal contours for this external contour
                int ChildIndex = Hierarchy[i][2]; // First child circuit
                while (ChildIndex != -1)
                {
                    if (!Processed[ChildIndex])
                    {
                        Group.InnerContours.push_back(Contours[ChildIndex]);
                        Processed[ChildIndex] = true;
                    }
                    ChildIndex = Hierarchy[ChildIndex][0]; // The next contour is at the same level
                }

                // Filter by area - remove too small contours
                double Area = cv::contourArea(Group.OtherContour);
                if (Area > 10.0) // Minimum area - 10 pixels
                {
                    Groups.push_back(Group);
                }
            }

            return Groups;
        }
    }
};

void SaveAdvancedContours(const std::vector<ContourGroup>& ContourGroups, const std::string& BaseName)
{
    AdvancedContourDetector Detector;
    std::string Filename = "advanced_contours_" + BaseName + ".txt";
    Detector.SaveContoursGroupToFile(ContourGroups, Filename);
}

int main(int argc, char* argv[])
{
    // 1. Create a test image with shapes
    createTestShapes("test_shapes.png");
    std::cout << "Created test shapes image" << "\n";

    // 2. Analyze the contours on the test image and save the data
    SimpleContourDetector contourDetector;
    auto contours = contourDetector.detectAllContours("test_shapes.png");

    // Saving contour information to files for the test image
    SaveContoursToFiles(contours, "test_shapes");
    std::cout << "Contour data for test shapes saved to files" << "\n";

    // 3. Working with fonts
    FontManager fontManager;
    fontManager.SetNormalizedSize(200, 200);

    // Loading or creating a normalized font
    std::string fontSourcePath = "Fonts/MyCustomFont";
    if (std::filesystem::exists(fontSourcePath))
    {
        std::cout << "Processing font: " << fontSourcePath << "\n";
        if (fontManager.LoadAndCreateNormalizedFont("MyCustomFont", fontSourcePath))
        {
            // Rendering test texts
            std::vector<std::string> testTexts;

            std::ifstream Input("Input\\Input.txt");
            if (Input.is_open())
            {
                std::cout << "Open File: Input/Input.txt\n\n";
                std::string Line;
                while (std::getline(Input, Line))
                {
                    testTexts.push_back(Line);
                }
            }
            else
            {
                std::cout << "Error with open file: Input/Input.txt\n\n";
            }

            std::cout << "Text to render:\n\n";

            for (const auto& text : testTexts)
            {
                std::cout << text << "\n";
            }

            for (const auto& text : testTexts)
            {
                std::cout << "\n=== Processing text: '" << text << "' ===\n";

                // Rendering text
                cv::Mat result = fontManager.RenderText(text, "MyCustomFont_Normalized");

                if (!result.empty())
                {
                    // Saving the image
                    std::string safeText = text;
                    std::replace(safeText.begin(), safeText.end(), ' ', '_');
                    std::string filename = "rendered_" + safeText + ".png";
                    cv::imwrite(filename, result);
                    std::cout << "Saved rendered text: " << filename << "\n";

                    AdvancedContourDetector AdvancedDetector;
                    auto ContourGroup = AdvancedDetector.ExtractContourGroups(filename);
                    std::cout << "Found " << ContourGroup.size() << " contour groups in rendered text\n";

                    // Preserve extended contours
                    SaveAdvancedContours(ContourGroup, "rendered_" + safeText);

                    GenerateTestGcode("all_contours_data_rendered_" + filename);
                }
            }
        }
    }
    else
    {
        std::cout << "Font source not found: " << fontSourcePath << "\n";
        std::cout << "Please create the font directory structure manually." << "\n";
    }       

    return 0;
}