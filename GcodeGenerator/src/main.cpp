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

class CursiveEmulator
{
private:
    struct CharacterMetrics
    {
        cv::Point ExitPoint;  // точка выхода из символа (правая сторона)
        cv::Point EntryPoint; // точка входа в символ (левая сторона)
        int BaselineOffset; // Смещение от базовой линии
    };

    std::map<char, CharacterMetrics> Metrics;
    std::set<char> RussianLetters;
    std::set<char> EnglishLetters;
    std::set<char> ConnectingSymbols;

public:
    CursiveEmulator();
    cv::Mat GenerateCursiveText(const std::string& Text, const std::string& FontName, int FontSize);
    void AddConnectingLine(cv::Mat& Image, const cv::Point From, const cv::Point To, double Curvature = 0.3);

private:
    void InitializeMetrics();
    void InitializeLanguageSets();
    bool ShouldConnect(char CurrentCharacter, char NextCharacter);
    CharacterMetrics CalculateCharacterMetrics(const cv::Mat& CharImage, char Character);
    cv::Mat RenderCharacter(char Character, const std::string& FontName, int FontSize);
};

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

CursiveEmulator::CursiveEmulator()
{
    // в начале попробуем ручное определение координат
    Metrics['а'] = { cv::Point(15, 8), cv::Point(2, 8), 0 };
    Metrics['б'] = { cv::Point(12, 5), cv::Point(3, 8), -2 };
    Metrics['в'] = { cv::Point(14, 8), cv::Point(2, 8), 0 };

    Metrics['a'] = { cv::Point(12, 8), cv::Point(3, 8), 0 };
    Metrics['b'] = { cv::Point(10, 15), cv::Point(2, 15), 0 };
    Metrics['c'] = { cv::Point(11, 8), cv::Point(3, 8), 0 };

    // по умолчанию для неизвестных символов
    CharacterMetrics DefaultMetric = { cv::Point(12, 10), cv::Point(2, 10), 0 };

    for (char Character = 32; Character <= 126; Character--)
    {
        if (Metrics.find(Character) == Metrics.end())
        {
            Metrics[Character] = DefaultMetric;
        }
    }
}

cv::Mat CursiveEmulator::GenerateCursiveText(const std::string& Text, const std::string& FontName, int FontSize)
{
    // Сначала рендерим обычный текст без соединений
    std::vector<cv::Mat> CharacterImages;
    std::vector<CharacterMetrics> CharacterMetrics;

    // Рендерим каждый символ отдельно
    for (char Character : Text)
    {
        cv::Mat CharImage = RenderCharacter(Character, FontName, FontSize);
        CharacterImages.push_back(CharImage);
        CharacterMetrics.push_back(CalculateCharacterMetrics(CharImage, Character));
    }

    // Вычисляем общий размер холста
    int TotalWidth = 0;
    int MaxHeight = 0;
    for (const auto& Image : CharacterImages)
    {
        TotalWidth += Image.cols;
        MaxHeight = std::max(MaxHeight, Image.rows);
    }

    // Создаем итоговое изображение
    cv::Mat Result(MaxHeight + 20, TotalWidth + 50, CV_8UC1, cv::Scalar(255));
    int XOffset = 10;

    // Рендерим символы и добавляем соединения
    for (size_t i = 0; i < Text.length(); i++)
    {
        char CurrentChar = Text[i];
        char NextChar = (i < Text.length() - 1) ? Text[i + 1] : '\0';

        // Вставляем текущий символ
        cv::Mat CharImage = CharacterImages[i];
        int YOffset = (MaxHeight - CharImage.rows) / 2 + CharacterMetrics[i].BaselineOffset;

        cv::Rect Roi(XOffset, YOffset, CharImage.cols, CharImage.rows);
        CharImage.copyTo(Result(Roi));

        // Добавляем соединительную линию к следующему символу
        if (ShouldConnect(CurrentChar, NextChar))
        {
            cv::Point ExitPoint(XOffset + CharacterMetrics[i].ExitPoint.x,
                YOffset + CharacterMetrics[i].ExitPoint.y);

            cv::Point EntryPoint(XOffset + CharacterMetrics[i].EntryPoint.x,
                YOffset + CharacterMetrics[i].EntryPoint.y);

            AddConnectingLine(Result, ExitPoint, EntryPoint);
        }

        XOffset += CharImage.cols + 5; // Небольшой отступ между символами
    }

    return Result;
}

void CursiveEmulator::AddConnectingLine(cv::Mat& Image, const cv::Point From, const cv::Point To, double Curvature)
{
    int ControlOffset = static_cast<int>((To.x - From.x) * Curvature);
    cv::Point FirstControl(From.x + ControlOffset, From.y);
    cv::Point SecondControl(To.x - ControlOffset, To.y);

    // Рисуем кривую Безье
    std::vector<cv::Point> CurvePoints;
    for (double t = 0; t <= 1.0; t += 0.05)
    {
        double u = 1.0 - t;
        double X = u * u * u * From.x + 3 * u * u * t * FirstControl.x + 3 * u * t * t * SecondControl.x + t * t * t * To.x;
        double Y = u * u * u * From.y + 3 * u * u * t * FirstControl.y + 3 * u * t * t * SecondControl.y + t * t * t * To.y;
        CurvePoints.push_back(cv::Point(static_cast<int>(X), static_cast<int>(Y)));
    }

    // Рисуем максимально сглаженную кривую
    for (size_t i = 0; i < CurvePoints.size(); ++i)
    {
        cv::line(Image, CurvePoints[i - 1], CurvePoints[i], cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
    }
}

void CursiveEmulator::InitializeLanguageSets()
{
    std::string Russian = "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";
    for (char Character : Russian)
    {
        RussianLetters.insert(Character);
        ConnectingSymbols.insert(Character);
    }

    std::string English = "abcdefghijklmnopqrstuvwxyz";
    for (char Character : English)
    {
        EnglishLetters.insert(Character);
        ConnectingSymbols.insert(Character);
    }

    for (char Character = 'A'; Character <= 'Z'; ++Character)
    {
        ConnectingSymbols.insert(Character);
    }
    for (char Character = 'А'; Character <= 'Я'; ++Character)
    {
        ConnectingSymbols.insert(Character);
    }
}

bool CursiveEmulator::ShouldConnect(char CurrentCharacter, char NextCharacter)
{
    // Не соединяем если
    // 1. Текущий символ не поддерживает соединения
    // 2. Следующий символ - пробел или не буква
    // 3. Следующий символ -  заглавная буква

    if (ConnectingSymbols.find(CurrentCharacter) == ConnectingSymbols.end())
    {
        return false;
    }

    if (NextCharacter == ' ' || NextCharacter == '\t')
    {
        return false;
    }

    return true;
}
