// Tested functions: remove after tests
// Function to obtain the skeleton of a binary image
cv::Mat skeletonize(const cv::Mat& binary) {
    cv::Mat skel(binary.size(), CV_8UC1, cv::Scalar(0));
    cv::Mat temp, eroded;
    cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));

    bool done;
    do {
        cv::morphologyEx(binary, temp, cv::MORPH_OPEN, element);
        cv::bitwise_not(temp, temp);
        cv::bitwise_and(binary, temp, temp);
        cv::bitwise_or(skel, temp, skel);
        cv::erode(binary, binary, element);

        double max;
        cv::minMaxLoc(binary, 0, &max);
        done = (max == 0);
    } while (!done);

    return skel;
}

// Function for obtaining thin outlines of characters
std::vector<std::vector<cv::Point>> getThinContours(const cv::Mat& image, int penWidth) {
    // 1. Image binarization
    cv::Mat gray, binary;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = image.clone();
    }
    cv::threshold(gray, binary, 127, 255, cv::THRESH_BINARY_INV);

    // 2. Morphological closure for connecting close contours
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(penWidth / 2, penWidth / 2));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);

    // 3. Getting the skeleton
    cv::Mat skeleton = skeletonize(binary);

    // 4. Search for skeletal contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skeleton, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    return contours;
}