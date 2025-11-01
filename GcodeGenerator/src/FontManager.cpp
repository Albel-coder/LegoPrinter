#include "FontManager.h"

cv::Mat ConvertTo4Channels(const cv::Mat& Input)
{
	if (Input.channels() == 4)
	{
		return Input.clone();
	}
	else if (Input.channels() == 3)
	{
		cv::Mat result;
		cv::cvtColor(Input, result, cv::COLOR_BGR2BGRA);
		return result;
	}
	else if (Input.channels() == 1)
	{
		cv::Mat result;
		cv::cvtColor(Input, result, cv::COLOR_GRAY2BGRA);
		return result;
	}

	// For an unknown format, we create a transparent image
	cv::Mat result(Input.rows, Input.cols, CV_8UC4, cv::Scalar(255, 255, 255, 0));
	return result;
}

cv::Mat FontManager::NormalizeCharacterImage(const cv::Mat& InputImage)
{
	// We use a large size to maintain quality
	return FitCharacterToSize(InputImage, NormalizedWidth, NormalizedHeight);
}

cv::Mat FontManager::FitCharacterToSize(const cv::Mat& CharacterImage, int TargetWidth, int TargetHeight)
{
	if (CharacterImage.empty())
	{
		cv::Mat Result(TargetHeight, TargetWidth, CV_8UC3, cv::Scalar(255, 255, 255));
		return Result;
	}

	// Convert to 3 channels (BGR) for a white background
	cv::Mat InputBGR;
	if (CharacterImage.channels() == 4)
	{
		cv::cvtColor(CharacterImage, InputBGR, cv::COLOR_BGRA2BGR);
	}
	else if (CharacterImage.channels() == 1)
	{
		cv::cvtColor(CharacterImage, InputBGR, cv::COLOR_GRAY2BGR);
	}
	else
	{
		InputBGR = CharacterImage.clone();
	}

	// Find the exact boundaries of the symbol (ignore the white background)
	cv::Mat Gray;
	cv::Mat Binary;
	cv::cvtColor(InputBGR, Gray, cv::COLOR_BGR2GRAY);

	// Invert and binarize - consider the symbol to be dark on a white background
	cv::Mat Inverted;
	cv::bitwise_not(Gray, Inverted);
	cv::threshold(Inverted, Binary, 10, 255, cv::THRESH_BINARY);

	// Finding contours
	std::vector<std::vector<cv::Point>> Contours;
	cv::findContours(Binary, Contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (Contours.empty())
	{
		// If there are no contours, return a white image
		cv::Mat Result(TargetHeight, TargetWidth, CV_8UC3, cv::Scalar(255, 255, 255));
		return Result;
	}

	// If the bounding rect is too small, use the original size.
	cv::Rect BoundingRect = cv::boundingRect(Contours[0]);
	for (size_t i = 1; i < Contours.size(); i++)
	{
		BoundingRect |= cv::boundingRect(Contours[i]);
	}

	// We trim to the bounding rect - this removes the empty spaces around the symbol.
	cv::Mat Cropped = InputBGR(BoundingRect);

	// Now we scale it so that the symbol takes up most of the target size.
	double ScaleX = (double)TargetWidth / Cropped.cols;
	double ScaleY = (double)TargetHeight / Cropped.rows;
	double Scale = std::min(ScaleX, ScaleY);

	// If the symbol is already smaller than the target size, do not increase it much
	Scale = std::min(Scale, 1.5); // Maximum magnification 1.5 times

	int NewWidth = static_cast<int>(Cropped.cols * Scale);
	int NewHeight = static_cast<int>(Cropped.rows * Scale);

	// Create a result with a white background
	cv::Mat Result(TargetHeight, TargetWidth, CV_8UC3, cv::Scalar(255, 255, 255));

	// Centering the symbol
	int X = (TargetWidth - NewWidth) / 2;
	int Y = (TargetHeight - NewHeight) / 2;

	// Scaling with high-quality interpolation
	cv::Mat Resized;
	if (NewWidth > 0 && NewHeight > 0)
	{
		if (Scale > 1.0)
		{
			cv::resize(Cropped, Resized, cv::Size(NewWidth, NewHeight), 0, 0, cv::INTER_CUBIC);
		}
		else
		{
			cv::resize(Cropped, Resized, cv::Size(NewWidth, NewHeight), 0, 0, cv::INTER_AREA);
		}

		// Copy to the result
		if (X >= 0 && Y >= 0 && X + NewWidth <= Result.cols && Y + NewHeight <= Result.rows)
		{
			Resized.copyTo(Result(cv::Rect(X, Y, NewWidth, NewHeight)));
		}
	}

	return Result;
}

bool FontManager::CreateNormalizedFont(const std::string& SourceFontFolder, const std::string& TargetFontFolder)
{
	std::cout << "Creating normalized font from: " << SourceFontFolder << " to: " << TargetFontFolder << "\n";

	// Create a target directory
	std::filesystem::create_directories(TargetFontFolder);

	// Load the source font mapping file
	std::ifstream SourceMapping(SourceFontFolder + "/Mapping.txt");
	if (!SourceMapping.is_open())
	{
		std::cout << "Error: could not open source mapping file: " << SourceFontFolder + "/Mapping.txt\n";
		return false;
	}

	std::ofstream TargetMapping(TargetFontFolder + "/Mapping.txt");
	if (!TargetMapping.is_open())
	{
		std::cout << "Error: could not create target mapping file: " << TargetFontFolder << "/Mapping.txt\n";
		return false;
	}

	std::string Line;
	int ProcessedCount = 0;
	int ErrorCount = 0;

	while (std::getline(SourceMapping, Line))
	{
		if (Line.empty() || Line[0] == '#')
		{
			// Saving comments
			TargetMapping << Line << "\n";
			continue;
		}

		std::istringstream Stream(Line);
		char Character;
		std::string PNGFilename;

		if (Stream >> Character >> PNGFilename)
		{
			std::string SourcePath = SourceFontFolder + "/" + PNGFilename;
			std::string TargetPath = TargetFontFolder + "/" + PNGFilename;

			if (std::filesystem::exists(SourcePath))
			{
				cv::Mat OriginImage = cv::imread(SourcePath, cv::IMREAD_UNCHANGED);

				if (!OriginImage.empty())
				{
					std::cout << "Processing character '" << Character << "' from " << SourcePath
						<< " (" << OriginImage.cols << "x" << OriginImage.rows << ")\n";

					// Normalizing the image
					cv::Mat NormalizedImage = NormalizeCharacterImage(OriginImage);

					// Saving the normalized image
					if (cv::imwrite(TargetPath, NormalizedImage))
					{
						// Write to the mapping file
						TargetMapping << Character << " " << PNGFilename << "\n";
						ProcessedCount++;

						std::cout << "  Normalized to: " << NormalizedImage.cols << "x" << NormalizedImage.rows << "\n";
					}
					else
					{
						std::cout << "Failed to save normalized character: " << TargetPath << "\n";
						ErrorCount++;
					}
				}
				else
				{
					std::cout << "Failed to load character image: " << SourcePath << "\n";
					ErrorCount++;
				}
			}
			else
			{
				std::cout << "Source character image not found: " << SourcePath << "\n";
				ErrorCount++;
			}
		}
		else
		{
			std::cout << "Invalid mapping line: " << Line << "\n";
			ErrorCount++;
		}
	}

	SourceMapping.close();
	TargetMapping.close();

	std::cout << "Successfully created normalized font with " << ProcessedCount << " characters";
	if (ErrorCount > 0)
	{
		std::cout << " (" << ErrorCount << " errors)";
	}
	std::cout << "\n";

	return ProcessedCount > 0;
}

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

bool FontManager::LoadAndCreateNormalizedFont(const std::string& FontName, const std::string& SourcePath)
{
	std::string NormalizedPath = SourcePath + "_Normalized";

	// Checking if a normalized font already exists
	if (std::filesystem::exists(NormalizedPath + "/Mapping.txt"))
	{
		std::cout << "Loading existing normalized font: " << NormalizedPath << "\n";
		LoadCustomFont(NormalizedPath);
		return true;
	}
	else
	{
		// Create a normalized font or underline
		std::cout << "Creating new normalized font from: " << SourcePath << "\n";
		if (CreateNormalizedFont(SourcePath, NormalizedPath))
		{
			LoadCustomFont(NormalizedPath);
			return true;
		}
		else
		{
			std::cout << "Failed to create normalized font\n";
			return false;
		}
	}
}

void FontManager::SetNormalizedSize(int Width, int Height)
{
	NormalizedWidth = Width;
	NormalizedHeight = Height;
}

cv::Mat FontManager::RenderWithCustomFont(const std::string& Text, const std::string FontName)
{
	std::cout << "RenderWithCustomFont: '" << Text << "' with font " << FontName << "\n";

	if (Text.empty())
	{
		std::cout << "Warning: Empty text provided\n";
		return cv::Mat(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
	}

	try
	{
		// Calculate image size
		int TotalWidth = 0;
		int MaxHeight = 0;
		std::vector<cv::Mat> CharacterImages;

		// Firstly: calculating size and load image
		for (char Character : Text)
		{
			cv::Mat CharImage = LoadCharacterImage(Character, FontName);

			CharacterImages.push_back(CharImage);
			TotalWidth += CharImage.cols;
			MaxHeight = std::max(MaxHeight, CharImage.rows);

			std::cout << "Character '" << Character << "' size: " << CharImage.cols << "x" << CharImage.rows << "\n";
		}

		// Add spaces between characters
		int spacing = MaxHeight / 10; // 10% of the height as an indentation
		TotalWidth += (Text.length() - 1) * spacing;

		// Create result image with white background
		cv::Mat Result(MaxHeight, TotalWidth, CV_8UC3, cv::Scalar(255, 255, 255));
		int XOffset = 0;

		// Render each character
		for (size_t i = 0; i < Text.length(); i++)
		{
			cv::Mat& CharImage = CharacterImages[i];

			if (XOffset + CharImage.cols > Result.cols)
			{
				std::cout << "Warning: Character exceeds image width at index " << i << "\n";
				continue;
			}

			int YOffset = (MaxHeight - CharImage.rows) / 2;

			cv::Rect Roi(XOffset, YOffset, CharImage.cols, CharImage.rows);
			if (Roi.x + Roi.width <= Result.cols && Roi.y + Roi.height <= Result.rows)
			{
				// Easy copying since both images are now in BGR
				CharImage.copyTo(Result(Roi));
			}

			XOffset += CharImage.cols + spacing;
		}

		return Result;
	}
	catch (const std::exception& ex)
	{
		std::cout << "Exception in RenderWithCustomFont: " << ex.what() << "\n";
		return cv::Mat(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
	}
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

	// Normalizing the size (now with a white background and proper removal of empty spaces)
	CharImage = FitCharacterToSize(CharImage, NormalizedWidth, NormalizedHeight);

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
