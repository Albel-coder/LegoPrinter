#include "FontManager.h"

FontManager::FontManager()
{
	InitializeBuiltinFonts();
}

void FontManager::InitializeBuiltinFonts()
{
	BuiltinFonts["Arial"] = { "Arial", 20, false, ""};
}

void FontManager::LoadCustomFont(const std::string& FontFolder)
{
	std::cout << "Starting load custom font in " << FontFolder << "\n";

	std::ifstream MappingFile(FontFolder + "/Mapping.txt");
	if (!MappingFile.is_open())
	{
		std::cout << "Error: could not open mapping file in " << FontFolder << "\n";
		return;
	}

	std::string Line;
	char Character;
	std::string PngFilename;
	while (std::getline(MappingFile, Line))
	{
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::istringstream String(Line);
		
		if (String >> Character >> PngFilename)
		{
			CharToPng[Character] = FontFolder + "/" + PngFilename;
			std::cout << "Mapped '" << Character << "' to " << CharToPng[Character] << "\n";
		}
		else
		{
			std::cout << "Warning: invalid mapping line: " << Line << "\n";
		}
	}

	// Check is Png file existing
	for (const auto& [Character, Filename] : CharToPng)
	{
		if (!std::filesystem::exists(Filename))
		{
			std::cout << "Warning: Png file not found for character '" << Character << "':" << Filename << "\n";
		}
		else
		{
			std::cout << "Verified: " << Filename << " exists\n";
		}
	}

	// Add custom font configuration
	std::string FontName = std::filesystem::path(FontFolder).filename().string();
	CustomFonts[FontName] = { FontName, 20, true, FontFolder };

	std::cout << "Loaded custom font :" << FontName << " with " << CharToPng.size() << " characters";
}

cv::Mat FontManager::RenderText(const std::string& Text, const std::string FontName)
{
	std::cout << "Rendering text: " << Text << " with font: " << FontName << "\n";

	// Check is custom font existing
	if (CustomFonts.find(FontName) != CustomFonts.end())
	{
		std::cout << "Using custom font: " << FontName << "\n";
		return RenderWithCustomFont(Text, FontName);
	}
	else if (BuiltinFonts.find(FontName) != BuiltinFonts.end())
	{
		std::cout << "Using build-in font: " << FontName << "\n";
		return RenderWithBuiltinFont(Text, FontName);
	}
	else
	{
		std::cout << "Font not found: " << FontName << ", using default Arial\n";
		return RenderWithBuiltinFont(Text, "Arial");
	}
}

cv::Mat FontManager::RenderWithCustomFont(const std::string& Text, const std::string FontName)
{
	// Calculate image size
	int TotalWidth = 0;
	int MaxHeight = 0;
	std::vector<cv::Mat> CharacterImages;
	std::vector<cv::Size> CharacterSizes;
	cv::Mat CharImage;

	// Firstly: calculating size and load image
	for (char Character : Text)
	{
		CharImage = LoadCharacterImage(Character, FontName);
		CharacterImages.push_back(CharImage);
		CharacterSizes.push_back(CharImage.size());

		TotalWidth += CharImage.cols;
		MaxHeight = std::max(MaxHeight, CharImage.rows);

		std::cout << "Character '" << Character << "' size: " << CharImage.cols << "x" << CharImage.rows << "\n";
	}

	// Add spaces between characters
	TotalWidth += (Text.length() - 1) * 2;

	// Create result image
	cv::Mat Result(MaxHeight, TotalWidth, CV_8UC4, cv::Scalar(255, 255, 255, 0));
	int XOffset = 0;

	// Second: render every character
	for (size_t i = 0; i < Text.length(); i++)
	{
		cv::Mat& CharImage = CharacterImages[i];
		int YOffset = (MaxHeight - CharImage.rows) / 2;

		// Embed character in result image
		if (CharImage.channels() == 3)
		{
			// RGB image
			CharImage.copyTo(Result(cv::Rect(XOffset, YOffset, CharImage.cols, CharImage.rows)));
		}
		else if (CharImage.channels() == 4)
		{
			// RGBA image
			cv::Mat RGBChar;
			cv::cvtColor(CharImage, RGBChar, cv::COLOR_BGRA2BGR);
			RGBChar.copyTo(Result(cv::Rect(XOffset, YOffset, CharImage.cols, CharImage.rows)));
		}
		else if (CharImage.channels() == 1)
		{
			// Grayscale - convert to RGB
			cv::Mat RGBChar;
			cv::cvtColor(CharImage, RGBChar, cv::COLOR_GRAY2BGR);
			RGBChar.copyTo(Result(cv::Rect(XOffset, YOffset, CharImage.cols, CharImage.rows)));
		}

		XOffset += CharImage.cols + 2;
	}

	return Result;
}

cv::Mat FontManager::LoadCharacterImage(char Character, const std::string& FontName)
{
	// Check cache
	if (FontCache[FontName].find(Character) != FontCache[FontName].end())
	{
		return FontCache[FontName][Character].clone();
	}

	cv::Mat CharImage;

	if (CharToPng.find(Character) != CharToPng.end())
	{
		CharImage = cv::imread(CharToPng[Character], cv::IMREAD_UNCHANGED);
		if (CharImage.empty())
		{
			std::cout << "Failed to load character '" << Character << "' from " << CharToPng[Character] << "\n";
			CharImage = RenderWithBuiltinFont(std::string(1, Character), "Arial");
		}
	}
	else
	{
		std::cout << "Character '" << Character << "' not found in custom font, using fallback\n";
		CharImage = RenderWithBuiltinFont(std::string(1, Character), "Arial");
	}

	// Save in cache
	FontCache[FontName][Character] = CharImage.clone();
	return CharImage;
}

cv::Mat FontManager::RenderWithBuiltinFont(const std::string& Text, const std::string FontName)
{
	auto& Config = BuiltinFonts[FontName];

	int FontFace = cv::FONT_HERSHEY_SIMPLEX;
	
	double FontScale = Config.size / 10.0;
	int Thickness = 2;

	// Calculating text size
	int Baseline = 0;
	cv::Size TextSize = cv::getTextSize(Text, FontFace, FontScale, Thickness, &Baseline);

	// Create image with little capacity
	cv::Mat Image(TextSize.height + Baseline + 20, TextSize.width + 20, CV_8UC4, cv::Scalar(255, 255, 255, 0));

	// Render text
	cv::putText(Image, Text, cv::Point(10, TextSize.height + 10),
		FontFace, FontScale, cv::Scalar(0, 0, 0, 255), Thickness);

	return Image;
}
