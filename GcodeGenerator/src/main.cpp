#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <map>
#include <ctype.h>
#include <filesystem>

#include "FontManager.h"

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

void SaveContoursToFiles(const std::vector<ContourInfo>& contours, const std::string& baseName)
{
    std::string simpleFileName = "simple_contours_" + baseName + ".txt";
    std::string detailedFileName = "all_contours_data_" + baseName + ".txt";

    std::ofstream simpleFile(simpleFileName);
    std::ofstream detailedFile(detailedFileName);

    if (simpleFile.is_open() && detailedFile.is_open())
    {
        simpleFile << "Total contours found: " << contours.size() << "\n\n";
        detailedFile << "Detailed contour analysis for: " << baseName << "\n";
        detailedFile << "Total contours: " << contours.size() << "\n";
        detailedFile << "========================================\n\n";

        for (size_t i = 0; i < contours.size(); i++)
        {
            const auto& contour = contours[i];

            // Simple information
            simpleFile << "Contour " << i << ":\n";
            simpleFile << "  Bounding Rect: [" << contour.boundingRect.x << ", " << contour.boundingRect.y
                << ", " << contour.boundingRect.width << "x" << contour.boundingRect.height << "]\n";
            simpleFile << "  Area: " << contour.area << "\n";
            simpleFile << "  Perimeter: " << contour.perimeter << "\n";
            simpleFile << "  Points: " << contour.points.size() << "\n\n";

            // Detailed information
            detailedFile << "CONTOUR " << i << ":\n";
            detailedFile << "Bounding Rect: x=" << contour.boundingRect.x
                << " y=" << contour.boundingRect.y
                << " width=" << contour.boundingRect.width
                << " height=" << contour.boundingRect.height << "\n";
            detailedFile << "Area: " << contour.area << "\n";
            detailedFile << "Perimeter: " << contour.perimeter << "\n";
            detailedFile << "Number of points: " << contour.points.size() << "\n";

            // We save all contour points
            detailedFile << "Points (x, y):\n";
            for (size_t j = 0; j < contour.points.size(); j++)
            {
                detailedFile << "  " << contour.points[j].x << ", " << contour.points[j].y;
                if (j < contour.points.size() - 1) 
                {
                    detailedFile << " ->";
                }
                detailedFile << "\n";
            }

            detailedFile << "----------------------------------------\n\n";
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
            std::vector<std::string> testTexts = 
            {
                "HELLO", "WORLD", "TEST", "123", "FONT"
            };

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

                    // Analyzing contours in rendered text
                    auto textContours = contourDetector.detectAllContours(filename);
                    std::cout << "Found " << textContours.size() << " contours in rendered text\n";

                    // Preserve outline information for this text
                    SaveContoursToFiles(textContours, "rendered_" + safeText);
                }
            }
        }
    }
    else
    {
        std::cout << "Font source not found: " << fontSourcePath << "\n";
        std::cout << "Please create the font directory structure manually." << "\n";
    }
}