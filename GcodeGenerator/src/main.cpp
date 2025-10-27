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
    double area;
    double perimeter;
};

class SimpleContourDetector 
{
public:
    std::vector<ContourInfo> detectAllContours(const std::string& imagePath) 
    {
        // Загрузка изображения
        cv::Mat image = cv::imread(imagePath);
        if (image.empty()) 
        {
            std::cerr << "Error: Could not load image " << imagePath << std::endl;
            return {};
        }

        std::cout << "Image: " << image.cols << "x" << image.rows << std::endl;

        // Простая предобработка
        cv::Mat gray, binary;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

        // Бинаризация - простой метод
        cv::threshold(gray, binary, 128, 255, cv::THRESH_BINARY);

        // Поиск ВСЕХ контуров
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(binary, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        std::cout << "Found " << contours.size() << " raw contours" << std::endl;

        // Преобразование в нашу структуру
        std::vector<ContourInfo> result;
        for (size_t i = 0; i < contours.size(); i++) 
        {
            if (contours[i].size() < 3) continue; // Минимум 3 точки

            ContourInfo info;
            info.points = contours[i];
            info.boundingRect = cv::boundingRect(contours[i]);
            info.area = cv::contourArea(contours[i]);
            info.perimeter = cv::arcLength(contours[i], true);

            // Базовый фильтр - убираем слишком маленькие контуры
            if (info.area > 5.0) 
            { // Минимальная площадь 5 пикселей
                result.push_back(info);
            }
        }

        std::cout << "After basic filtering: " << result.size() << " contours" << std::endl;

        // Визуализация
        visualizeContours(image, result, imagePath);

        return result;
    }

private:
    void visualizeContours(const cv::Mat& original,
        const std::vector<ContourInfo>& contours,
        const std::string& imagePath) {
        cv::Mat result = original.clone();

        // Рисуем все контуры
        for (const auto& contour : contours) 
        {
            // Случайный цвет для каждого контура
            cv::Scalar color(rand() % 256, rand() % 256, rand() % 256);

            // Рисуем bounding box
            cv::rectangle(result, contour.boundingRect, color, 2);

            // Рисуем сам контур
            std::vector<std::vector<cv::Point>> contoursToDraw = { contour.points };
            cv::drawContours(result, contoursToDraw, -1, color, 1);
        }

        // Добавляем информацию
        std::string info = "Contours: " + std::to_string(contours.size());
        cv::putText(result, info, cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);

        // Сохраняем результат
        size_t lastDot = imagePath.find_last_of(".");
        std::string resultPath = imagePath.substr(0, lastDot) + "_all_contours.jpg";
        cv::imwrite(resultPath, result);

        std::cout << "Visualization saved: " << resultPath << std::endl;
    }
};

// Function for create test png image with different characters
void createTestShapes(const std::string& outputPath) 
{
    cv::Mat image(600, 800, CV_8UC3, cv::Scalar(255, 255, 255));

    // Рисуем разные фигуры
    cv::rectangle(image, cv::Point(50, 50), cv::Point(200, 200), cv::Scalar(0, 0, 0), 2);
    cv::circle(image, cv::Point(300, 125), 75, cv::Scalar(0, 0, 0), 2);

        cv::ellipse(image, cv::Point(500, 125), cv::Size(100, 50), 0, 0, 360, cv::Scalar(0, 0, 0), 2);

    // Треугольник
    cv::Point triangle[3] = { cv::Point(100, 300), cv::Point(50, 400), cv::Point(150, 400) };
    cv::fillConvexPoly(image, triangle, 3, cv::Scalar(0, 0, 0));

    // Многоугольник
    cv::Point polygon[6] = 
    {
        cv::Point(250, 300), cv::Point(300, 250), cv::Point(350, 250),
        cv::Point(400, 300), cv::Point(350, 400), cv::Point(300, 400)
    };
    cv::polylines(image, std::vector<cv::Point>(polygon, polygon + 6), true, cv::Scalar(0, 0, 0), 2);

    // Текст тоже как набор контуров
    cv::putText(image, "G-CODE TEST", cv::Point(450, 350),
        cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);

    cv::imwrite(outputPath, image);
    std::cout << "Test shapes image created: " << outputPath << std::endl;
}

int main(int argc, char* argv[]) 
{
    FontManager MyFont;
    MyFont.LoadCustomFont("Fonts/MyCustomFont");

    std::string Font = "MyCustomFont";

    std::string Text = "Hello World!";

    try
    {
        cv::Mat Result = MyFont.RenderText(Text, Font);
        if (!Result.empty())
        {
            std::string Filename = "Result.png";
            cv::imwrite(Filename, Result);
            std::cout << "Saved: " << Filename << "\n";
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << "Error rendering text '" << Text << "': " << ex.what() << "\n";
    }

    return 0;
}