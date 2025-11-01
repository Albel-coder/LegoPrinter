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
    std::string Name;
    int size = 20;
    bool IsCustom = false;
    std::string Path; // For custom fonts

    FontConfig() : size(20), IsCustom(false) {}
    FontConfig(const std::string& name, int s, bool custom, const std::string& path)
        : Name(name), size(s), IsCustom(custom), Path(path) { }
};

class FontManager
{
private:
    std::map<std::string, FontConfig> BuiltinFonts;
    std::map<std::string, FontConfig> CustomFonts;
    std::map<char, std::string> CharToPng;
    std::map<std::string, std::map<char, cv::Mat>> FontCache;

    // Normalization Options
    int NormalizedWidth = 100;
    int NormalizedHeight = 100;
    cv::Scalar BackgroundColor = cv::Scalar(255, 255, 255);

    // Methods for normalization
    cv::Mat NormalizeCharacterImage(const cv::Mat& InputImage);
    cv::Mat FitCharacterToSize(const cv::Mat& CharacterImage, int TargetWidth, int TargetHeight);
    bool CreateNormalizedFont(const std::string& SourceFontFolder, const std::string& TargetFontFolder);

public:
    FontManager();
    void InitializeBuiltinFonts();
    void LoadCustomFont(const std::string& FontFolder);
    cv::Mat RenderText(const std::string& Text, const std::string FontName);

    // Methods for working with normalized fonts and underscores
    bool LoadAndCreateNormalizedFont(const std::string& FontName, const std::string& SourcePath);
    void SetNormalizedSize(int Width, int Height);

private:
    cv::Mat RenderWithCustomFont(const std::string& Text, const std::string FontName);
    cv::Mat LoadCharacterImage(char Character, const std::string& FontName);
    cv::Mat RenderWithBuiltinFont(const std::string& Text, const std::string FontName);
};
