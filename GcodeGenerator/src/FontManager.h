#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <map>
#include <ctype.h>
#include <fstream>
#include <filesystem>

struct FontConfig
{
    std::string name;
    int size = 20;
    bool isCustom = false;
    std::string path; // For custom fonts

    FontConfig() : size(20), isCustom(false) {}
    FontConfig(const std::string& name, int s, bool custom, const std::string& path)
        : name(name), size(s), isCustom(custom), path(path) { }
};

class FontManager
{
private:
    std::map<std::string, FontConfig> builtinFonts;
    std::map<std::string, FontConfig> customFonts;
    std::map<char, std::string> charToPng;
    std::map<std::string, std::map<char, cv::Mat>> fontCache;

    // Normalization Options
    int normalizedWidth = 100;
    int normalizedHeight = 100;
    cv::Scalar backgroundColor = cv::Scalar(255, 255, 255);

    // Methods for normalization
    cv::Mat normalizeCharacterImage(const cv::Mat& inputImage);
    cv::Mat fitCharacterToSize(const cv::Mat& characterImage, int targetWidth, int targetHeight);
    bool createNormalizedFont(const std::string& sourceFontFolder, const std::string& targetFontFolder);

public:
    FontManager();

    void initializeBuiltinFonts();
    void loadCustomFont(const std::string& fontFolder);
    cv::Mat renderText(const std::string& text, const std::string fontName);

    // Methods for working with normalized fonts and underscores
    bool loadAndCreateNormalizedFont(const std::string& fontName, const std::string& sourcePath);
    void setNormalizedSize(int width, int height);

private:
    cv::Mat renderWithCustomFont(const std::string& text, const std::string fontName);
    cv::Mat loadCharacterImage(char character, const std::string& fontName);
    cv::Mat renderWithBuiltinFont(const std::string& text, const std::string fontName);
};
