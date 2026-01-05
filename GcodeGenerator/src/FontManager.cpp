#include "FontManager.h"

cv::Mat convertTo4Channels(const cv::Mat& input) {
	if (input.channels() == 4) {
		return input.clone();
	}
	else if (input.channels() == 3) {
		cv::Mat result;
		cv::cvtColor(input, result, cv::COLOR_BGR2BGRA);
		return result;
	}
	else if (input.channels() == 1)	{
		cv::Mat result;
		cv::cvtColor(input, result, cv::COLOR_GRAY2BGRA);
		return result;
	}

	// For an unknown format, we create a transparent image
	cv::Mat result(input.rows, input.cols, CV_8UC4, cv::Scalar(255, 255, 255, 0));
	return result;
}

cv::Mat FontManager::normalizeCharacterImage(const cv::Mat& inputImage) {
	// We use a large size to maintain quality
	return fitCharacterToSize(inputImage, normalizedWidth, normalizedHeight);
}

cv::Mat FontManager::fitCharacterToSize(const cv::Mat& characterImage, int targetWidth, int targetHeight) {
	if (characterImage.empty())	{
		cv::Mat result(targetHeight, targetWidth, CV_8UC3, cv::Scalar(255, 255, 255));
		return result;
	}

	// Convert to 3 channels (BGR) for a white background
	cv::Mat inputBGR;
	if (characterImage.channels() == 4)	{
		cv::cvtColor(characterImage, inputBGR, cv::COLOR_BGRA2BGR);
	}
	else if (characterImage.channels() == 1) {
		cv::cvtColor(characterImage, inputBGR, cv::COLOR_GRAY2BGR);
	}
	else {
		inputBGR = characterImage.clone();
	}

	// Find the exact boundaries of the symbol (ignore the white background)
	cv::Mat gray;
	cv::Mat binary;
	cv::cvtColor(inputBGR, gray, cv::COLOR_BGR2GRAY);

	// Invert and binarize - consider the symbol to be dark on a white background
	cv::Mat inverted;
	cv::bitwise_not(gray, inverted);
	cv::threshold(inverted, binary, 10, 255, cv::THRESH_BINARY);

	// Finding contours
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (contours.empty()) {
		// If there are no contours, return a white image
		cv::Mat result(targetHeight, targetWidth, CV_8UC3, cv::Scalar(255, 255, 255));
		return result;
	}

	// If the bounding rect is too small, use the original size.
	cv::Rect boundingRect = cv::boundingRect(contours[0]);
	for (size_t i = 1; i < contours.size(); i++) {
		boundingRect |= cv::boundingRect(contours[i]);
	}

	// We trim to the bounding rect - this removes the empty spaces around the symbol.
	cv::Mat cropped = inputBGR(boundingRect);

	// Now we scale it so that the symbol takes up most of the target size.
	double scaleX = (double)targetWidth / cropped.cols;
	double scaleY = (double)targetHeight / cropped.rows;
	double scale = std::min(scaleX, scaleY);

	// If the symbol is already smaller than the target size, do not increase it much
	scale = std::min(scale, 1.5); // Maximum magnification 1.5 times

	int newWidth = static_cast<int>(cropped.cols * scale);
	int newHeight = static_cast<int>(cropped.rows * scale);

	// Create a result with a white background
	cv::Mat result(targetHeight, targetWidth, CV_8UC3, cv::Scalar(255, 255, 255));

	// Centering the symbol
	int x = (targetWidth - newWidth) / 2;
	int y = (targetHeight - newHeight) / 2;

	// Scaling with high-quality interpolation
	cv::Mat resized;
	if (newWidth > 0 && newHeight > 0) {
		if (scale > 1.0) {
			cv::resize(cropped, resized, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_CUBIC);
		}
		else {
			cv::resize(cropped, resized, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);
		}

		// Copy to the result
		if (x >= 0 && y >= 0 && x + newWidth <= result.cols && y + newHeight <= result.rows) {
			resized.copyTo(result(cv::Rect(x, y, newWidth, newHeight)));
		}
	}

	return result;
}

bool FontManager::createNormalizedFont(const std::string& sourceFontFolder, const std::string& targetFontFolder) {
	std::cout << "Creating normalized font from: " << sourceFontFolder << " to: " << targetFontFolder << "\n";

	// Create a target directory
	std::filesystem::create_directories(targetFontFolder);

	// Load the source font mapping file
	std::ifstream sourceMapping(sourceFontFolder + "/Mapping.txt");
	if (!sourceMapping.is_open()) {
		//std::cout << "Error: could not open source mapping file: " << sourceFontFolder + "/Mapping.txt\n";
		return false;
	}

	std::ofstream targetMapping(targetFontFolder + "/Mapping.txt");
	if (!targetMapping.is_open()) {
		//std::cout << "Error: could not create target mapping file: " << targetFontFolder << "/Mapping.txt\n";
		return false;
	}

	std::string line;
	int processedCount = 0;
	int errorCount = 0;

	while (std::getline(sourceMapping, line)) {
		if (line.empty() || line[0] == '#')	{
			// Saving comments
			targetMapping << line << "\n";
			continue;
		}

		std::istringstream stream(line);
		char character;
		std::string PNGFilename;

		if (stream >> character >> PNGFilename)	{
			std::string sourcePath = sourceFontFolder + "/" + PNGFilename;
			std::string targetPath = targetFontFolder + "/" + PNGFilename;

			if (std::filesystem::exists(sourcePath)) {
				cv::Mat originImage = cv::imread(sourcePath, cv::IMREAD_UNCHANGED);

				if (!originImage.empty()) {
					//std::cout << "Processing character '" << character << "' from " << sourcePath
					//	<< " (" << originImage.cols << "x" << originImage.rows << ")\n";

					// Normalizing the image
					cv::Mat normalizedImage = normalizeCharacterImage(originImage);

					// Saving the normalized image
					if (cv::imwrite(targetPath, normalizedImage)) {
						// Write to the mapping file
						targetMapping << character << " " << PNGFilename << "\n";
						processedCount++;

						//std::cout << "  Normalized to: " << normalizedImage.cols << "x" << normalizedImage.rows << "\n";
					}
					else {
						//std::cout << "Failed to save normalized character: " << targetPath << "\n";
						errorCount++;
					}
				}
				else {
					//std::cout << "Failed to load character image: " << sourcePath << "\n";
					errorCount++;
				}
			}
			else {
				//std::cout << "Source character image not found: " << sourcePath << "\n";
				errorCount++;
			}
		}
		else {
			//std::cout << "Invalid mapping line: " << line << "\n";
			errorCount++;
		}
	}

	sourceMapping.close();
	targetMapping.close();

	//std::cout << "Successfully created normalized font with " << processedCount << " characters";
	if (errorCount > 0)	{
		//std::cout << " (" << errorCount << " errors)";
	}
	//std::cout << "\n";

	return processedCount > 0;
}

FontManager::FontManager() {
	initializeBuiltinFonts();
}

void FontManager::initializeBuiltinFonts() {
	builtinFonts["Arial"] = { "Arial", 20, false, ""};
}

void FontManager::loadCustomFont(const std::string& fontFolder) {
	//std::cout << "Starting load custom font in " << fontFolder << "\n";

	std::ifstream mappingFile(fontFolder + "/Mapping.txt");
	if (!mappingFile.is_open())	{
		//std::cout << "Error: could not open mapping file in " << fontFolder << "\n";
		return;
	}

	std::string line;
	char character;
	std::string pngFilename;
	while (std::getline(mappingFile, line))	{
		if (line.empty() || line[0] == '#') {
			continue;
		}

		std::istringstream string(line);
		
		if (string >> character >> pngFilename)	{
			charToPng[character] = fontFolder + "/" + pngFilename;
			//std::cout << "Mapped '" << character << "' to " << charToPng[character] << "\n";
		}
		else {
			//std::cout << "Warning: invalid mapping line: " << line << "\n";
		}
	}

	// Check is Png file existing
	for (const auto& [character, filename] : charToPng) {
		if (!std::filesystem::exists(filename))	{
			//std::cout << "Warning: Png file not found for character '" << character << "':" << filename << "\n";
		}
		else {
			//std::cout << "Verified: " << filename << " exists\n";
		}
	}

	// Add custom font configuration
	std::string fontName = std::filesystem::path(fontFolder).filename().string();
	customFonts[fontName] = { fontName, 20, true, fontFolder };

	//std::cout << "Loaded custom font :" << fontName << " with " << charToPng.size() << " characters";
}

cv::Mat FontManager::renderText(const std::string& text, const std::string fontName) {
	//std::cout << "Rendering text: " << text << " with font: " << fontName << "\n";

	// Check is custom font existing
	if (customFonts.find(fontName) != customFonts.end()) {
		//std::cout << "Using custom font: " << fontName << "\n";
		return renderWithCustomFont(text, fontName);
	}
	else if (builtinFonts.find(fontName) != builtinFonts.end())	{
		//std::cout << "Using build-in font: " << fontName << "\n";
		return renderWithBuiltinFont(text, fontName);
	}
	else {
		//std::cout << "Font not found: " << fontName << ", using default Arial\n";
		return renderWithBuiltinFont(text, "Arial");
	}
}

bool FontManager::loadAndCreateNormalizedFont(const std::string& fontName, const std::string& sourcePath) {
	std::string normalizedPath = sourcePath + "_Normalized";

	// Checking if a normalized font already exists
	if (std::filesystem::exists(normalizedPath + "/Mapping.txt")) {
		//std::cout << "Loading existing normalized font: " << normalizedPath << "\n";
		loadCustomFont(normalizedPath);
		return true;
	}
	else {
		// Create a normalized font or underline
		//std::cout << "Creating new normalized font from: " << sourcePath << "\n";
		if (createNormalizedFont(sourcePath, normalizedPath)) {
			loadCustomFont(normalizedPath);
			return true;
		}
		else {
			//std::cout << "Failed to create normalized font\n";
			return false;
		}
	}
}

void FontManager::setNormalizedSize(int width, int height) {
	normalizedWidth = width;
	normalizedHeight = height;
}

cv::Mat FontManager::renderWithCustomFont(const std::string& text, const std::string fontName) {
	//std::cout << "RenderWithCustomFont: '" << text << "' with font " << fontName << "\n";

	if (text.empty()) {
		//std::cout << "Warning: Empty text provided\n";
		return cv::Mat(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
	}

	try	{
		// Calculate image size
		int totalWidth = 0;
		int maxHeight = 0;
		std::vector<cv::Mat> characterImages;

		// Firstly: calculating size and load image
		for (char character : text)	{
			cv::Mat charImage = loadCharacterImage(character, fontName);

			characterImages.push_back(charImage);
			totalWidth += charImage.cols;
			maxHeight = std::max(maxHeight, charImage.rows);

			//std::cout << "Character '" << character << "' size: " << charImage.cols << "x" << charImage.rows << "\n";
		}

		// Add spaces between characters
		int spacing = maxHeight / 10; // 10% of the height as an indentation
		totalWidth += (text.length() - 1) * spacing;

		// Create result image with white background
		cv::Mat result(maxHeight, totalWidth, CV_8UC3, cv::Scalar(255, 255, 255));
		int xOffset = 0;

		// Render each character
		for (size_t i = 0; i < text.length(); i++) {
			cv::Mat& charImage = characterImages[i];

			if (xOffset + charImage.cols > result.cols)	{
				//std::cout << "Warning: Character exceeds image width at index " << i << "\n";
				continue;
			}

			int yOffset = (maxHeight - charImage.rows) / 2;

			cv::Rect Roi(xOffset, yOffset, charImage.cols, charImage.rows);
			if (Roi.x + Roi.width <= result.cols && Roi.y + Roi.height <= result.rows) {
				// Easy copying since both images are now in BGR
				charImage.copyTo(result(Roi));
			}

			xOffset += charImage.cols + spacing;
		}

		return result;
	}
	catch (const std::exception& ex) {
		//std::cout << "Exception in RenderWithCustomFont: " << ex.what() << "\n";
		return cv::Mat(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
	}
}

cv::Mat FontManager::loadCharacterImage(char character, const std::string& fontName) {
	// Check cache
	if (fontCache[fontName].find(character) != fontCache[fontName].end()) {
		return fontCache[fontName][character].clone();
	}

	cv::Mat charImage;

	if (charToPng.find(character) != charToPng.end()) {
		charImage = cv::imread(charToPng[character], cv::IMREAD_UNCHANGED);

		if (charImage.empty()) {
			//std::cout << "Failed to load character '" << character << "' from " << charToPng[character] << "\n";
			charImage = renderWithBuiltinFont(std::string(1, character), "Arial");
		}
	}
	else {
		//std::cout << "Character '" << character << "' not found in custom font, using fallback\n";
		charImage = renderWithBuiltinFont(std::string(1, character), "Arial");
	}

	// Normalizing the size (now with a white background and proper removal of empty spaces)
	charImage = fitCharacterToSize(charImage, normalizedWidth, normalizedHeight);

	// Save in cache
	fontCache[fontName][character] = charImage.clone();
	return charImage;
}

cv::Mat FontManager::renderWithBuiltinFont(const std::string& text, const std::string fontName) {
	auto& config = builtinFonts[fontName];
	int fontFace = cv::FONT_HERSHEY_SIMPLEX;	
	double fontScale = config.size / 10.0;
	int thickness = 2;

	// Calculating text size
	int baseline = 0;
	cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);

	// Create image with little capacity
	cv::Mat image(textSize.height + baseline + 20, textSize.width + 20, CV_8UC4, cv::Scalar(255, 255, 255, 0));

	// Render text
	cv::putText(image, text, cv::Point(10, textSize.height + 10),
		fontFace, fontScale, cv::Scalar(0, 0, 0, 255), thickness);

	return image;
}
