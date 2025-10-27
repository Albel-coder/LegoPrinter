#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <map>
#include <ctype.h>
#include <fstream>
#include <filesystem>

struct FontConfig
{
    std::string Name;
    int size;
    bool IsCustom;
    std::string Path; // For custom fonts
};

class FontManager
{
private:
    std::map<std::string, FontConfig> BuiltinFonts;
    std::map<std::string, FontConfig> CustomFonts;
    std::map<char, std::string> CharToPng;
    std::map<std::string, std::map<char, cv::Mat>> FontCache;

public:
    FontManager();
    void InitializeBuiltinFonts();
    void LoadCustomFont(const std::string& FontFolder);
    cv::Mat RenderText(const std::string& Text, const std::string FontName);

private:
    cv::Mat RenderWithCustomFont(const std::string& Text, const std::string FontName);
    cv::Mat LoadCharacterImage(char Character, const std::string& FontName);
    cv::Mat RenderWithBuiltinFont(const std::string& Text, const std::string FontName);
};
