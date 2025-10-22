#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <map>
#include <ctype.h>
#include <filesystem>

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

// Функция для создания тестового изображения с разными фигурами
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

struct FontConfig
{
    std::string Name;
    int size;
    bool IsCustom;
    std::string Path; // Для кастомных шрифтов
};

class FontManager
{
private:
    std::map<std::string, FontConfig> BuiltinFonts;
    std::map<std::string, FontConfig> CustomFonts;
    std::map<char, std::string> CharToPng;

public:
    void LoadCustomFont(const std::string& FontFolder);
    cv::Mat RenderText(const std::string& Text, const std::string& FontName);
    cv::Mat RenderWithCustomFont(const std::string& Text, const std::string& FontName);

    cv::Mat RenderWithBuiltinFont(const std::string& Text, const std::string& FontName);
    cv::Mat RenderbuiltinCharacter(char Character, const std::string& FontName);
};

void FontManager::LoadCustomFont(const std::string& FontFolder)
{
    std::ifstream MappingFile(FontFolder + "/Mapping.txt");
    std::string Line;

    while (std::getline(MappingFile, Line))
    {
        std::istringstream String(Line);
        char Character;
        std::string PngFilename;

        if (String >> Character >> PngFilename)
        {
            CharToPng[Character] = FontFolder + "/";
        }
    }

    // Проверяем существование PNG файлов
    for (const auto& [Ch, Filename] : CharToPng)
    {
        if (!std::filesystem::exists(Filename))
        {
            std::cerr << "Warning: PNG file not found character '" << Ch << "': " << Filename << "\n";
        }
    }
}

cv::Mat FontManager::RenderText(const std::string& Text, const std::string& FontName)
{
    std::cout << "Render text...";
    // Определяем какой шрифт использовать
    bool UseCustomFont = (CustomFonts.find(FontName) != CustomFonts.end());

    if (UseCustomFont)
    {
        std::cout << "Use custom font";
        return RenderWithCustomFont(Text, FontName);
    }
    else
    {
        std::cout << "Use basic font";
        return RenderWithBuiltinFont(Text, FontName);
    }
}

cv::Mat FontManager::RenderWithCustomFont(const std::string& Text, const std::string& FontName)
{
    // Рассчитываем размер итогового изображения
    int TotalWidth = 0;
    int MaxHeight = 0;

    // Первый проход: вычисляем размеры
    for (char Character : Text)
    {
        if (CharToPng.find(Character) != CharToPng.end())
        {
            cv::Mat CharImage = cv::imread(CharToPng[Character], cv::IMREAD_UNCHANGED);
            TotalWidth += CharImage.cols;
            MaxHeight = std::max(MaxHeight, CharImage.rows);
        }
        else
        {
            // Символ не найден в кастомном шрифте - используем встроенный как fallback
            cv::Mat FallbackChar = RenderbuiltinCharacter(Character, "Arial");
            TotalWidth += FallbackChar.cols;
            MaxHeight = std::max(MaxHeight, FallbackChar.rows);
        }
    }

    // Создаем итоговое изображение
    cv::Mat Result(MaxHeight, TotalWidth, CV_8UC4, cv::Scalar(255, 255, 255, 0));
    int XOffset = 0;

    // Второй проход: рендерим каждый символ
    cv::Mat CharImage;
    for (char Character : Text)
    {
        if (CharToPng.find(Character) != CharToPng.end())
        {
            CharImage = cv::imread(CharToPng[Character], cv::IMREAD_UNCHANGED);
        }
        else
        {
            CharImage = RenderbuiltinCharacter(Character, "Arial");
        }

        // Встраиваем символ в итоговое изображение
        CharImage.copyTo(Result(cv::Rect(XOffset,
            (MaxHeight - CharImage.rows) / 2,
            CharImage.cols,
            CharImage.rows)));

        XOffset += CharImage.cols;
    }

    return Result;
}

cv::Mat FontManager::RenderWithBuiltinFont(const std::string& Text, const std::string& FontName)
{
    auto& Config = BuiltinFonts[FontName];

    // Используем OpenCV для рендеринга текста
    int FontFace = cv::FONT_HERSHEY_SIMPLEX;
    double FontScale = Config.size / 10.0;
    int Thickness = 2;

    // Вычисляем размер текста
    cv::Size TextSize = cv::getTextSize(Text, FontFace, FontScale, Thickness, nullptr);

    // Создаем изображение с запасом
    cv::Mat Image(TextSize.height + 20, TextSize.width + 20, CV_8UC4, cv::Scalar(255, 255, 255, 0));

    // Рендерим текст
    cv::putText(Image, Text, cv::Point(10, TextSize.height + 10),
        FontFace, FontScale, cv::Scalar(0, 0, 0, 255), Thickness);

    return Image;
}

cv::Mat FontManager::RenderbuiltinCharacter(char Character, const std::string& FontName)
{
    return RenderWithBuiltinFont(std::string(1, Character), FontName);
}

int main(int argc, char* argv[]) 
{

    FontManager Font;

    // Загружаем кастомный шрифт
    Font.LoadCustomFont("Fonts/MyCustomFont");

    // Рендерим текст
    std::string Text = "Hello world!";
    cv::Mat Result = Font.RenderText(Text, "MyCustomFont");

    // Сохраняем результат
    cv::imwrite("Result.png", Result);

    return 0;
}
