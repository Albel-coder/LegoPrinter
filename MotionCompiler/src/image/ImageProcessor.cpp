#include "ImageProcessor.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace {

    double polygonArea(const std::vector<Point>& P) {
        if (P.size() < 3) return 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < P.size(); ++i) {
            const size_t j = (i + 1) % P.size();
            sum += P[i].x * P[j].y - P[j].x * P[i].y;
        }
        return std::abs(sum) * 0.5;
    }

} // namespace

ImageProcessor::ImageProcessor(Config config) : config(std::move(config)) {}

bool ImageProcessor::loadImage(const std::string& filename, cv::Mat& image) const {
    image = cv::imread(filename, cv::IMREAD_UNCHANGED);
    if (image.empty()) return false;

    if (image.depth() == CV_16U) {
        image.convertTo(image, CV_8U, 1.0 / 257.0);
    }
    else if (image.depth() != CV_8U) {
        image.convertTo(image, CV_8U);
    }

    if (image.channels() == 1) {
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    }
    else if (image.channels() == 4) {
        std::vector<cv::Mat> ch;
        cv::split(image, ch);

        cv::Mat alphaF;
        ch[3].convertTo(alphaF, CV_32F, 1.0 / 255.0);

        cv::Mat alpha3;
        cv::merge(std::vector<cv::Mat>{alphaF, alphaF, alphaF}, alpha3);

        cv::Mat rgbF;
        cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, rgbF);
        rgbF.convertTo(rgbF, CV_32F);

        cv::Mat ones(alpha3.size(), alpha3.type(), cv::Scalar::all(1.0));
        cv::Mat invAlpha = ones - alpha3;

        cv::Mat result = rgbF.mul(alpha3) + invAlpha * 255.0;
        result.convertTo(image, CV_8U);
    }
    return true;
}

cv::Mat ImageProcessor::buildField(const cv::Mat& image) const {
    if (image.empty()) return {};

    cv::Mat field;
    if (config.colorMode == ColorMode::Dark || config.colorMode == ColorMode::Black) {
        cv::Mat v;
        if (config.colorMode == ColorMode::Dark) {
            cv::cvtColor(image, v, cv::COLOR_BGR2GRAY);
        }
        else {
            std::vector<cv::Mat> ch;
            cv::split(image, ch);
            cv::max(ch[0], ch[1], v);
            cv::max(v, ch[2], v);
        }
        v.convertTo(field, CV_32F);
        field = (cv::Scalar(config.blackThreshold) - field) * (1.0 / 255.0) + 0.5;
    }
    else {
        cv::Mat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

        std::vector<cv::Mat> hsvCh;
        cv::split(hsv, hsvCh);
        const cv::Mat& hue = hsvCh[0];

        cv::Mat s, v;
        hsvCh[1].convertTo(s, CV_32F, 1.0 / 255.0);
        hsvCh[2].convertTo(v, CV_32F, 1.0 / 255.0);
        cv::Mat sv = s.mul(v);

        cv::Mat gateLow, gateHigh, gate;
        cv::compare(hue, config.redHueLow, gateLow, cv::CMP_LE);
        cv::compare(hue, config.redHueHigh, gateHigh, cv::CMP_GE);
        cv::bitwise_or(gateLow, gateHigh, gate);

        cv::Mat gateF;
        gate.convertTo(gateF, CV_32F, 1.0 / 255.0);

        field = sv.mul(gateF);
    }

    cv::max(field, 0.0, field);
    cv::min(field, 1.0, field);
    return field;
}

ImageProfile ImageProcessor::extractContours(const cv::Mat& field, double scale) const {
    ImageProfile result;
    if (field.empty()) return result;

    const int H = field.rows;
    const int W = field.cols;
    const int up = std::max(1, std::min(config.upscale, config.maxUpscaledSide / std::max(H, W)));

    cv::Mat big;
    if (up > 1) {
        cv::resize(field, big, cv::Size(), up, up, cv::INTER_CUBIC);
    }
    else {
        big = field;
    }

    cv::Mat mask;
    cv::compare(big, 0.5, mask, cv::CMP_GE);

    std::vector<std::vector<cv::Point>> cvContours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(mask, cvContours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);

    if (cvContours.empty()) return result;

    const double k = scale / static_cast<double>(up);
    const double H_up = static_cast<double>(H) * up;

    struct RawItem {
        size_t src;
        Contour contour;
    };
    std::vector<RawItem> raw;
    raw.reserve(cvContours.size());

    for (size_t i = 0; i < cvContours.size(); ++i) {
        const auto& c = cvContours[i];
        if (c.size() < 4) continue;

        Contour contour;
        contour.closed = true;
        contour.parent = hierarchy[i][3];
        contour.sourcePoints.reserve(c.size());
        contour.points.reserve(c.size());

        for (const auto& p : c) {
            Point pt;
            pt.x = (static_cast<double>(p.x) + 0.5) * k + config.originX;
            pt.y = config.flipY
                ? (H_up - (static_cast<double>(p.y) + 0.5)) * k + config.originY
                : (static_cast<double>(p.y) + 0.5) * k + config.originY;
            contour.sourcePoints.push_back(pt);
            contour.points.push_back(pt);
        }

        contour.area = polygonArea(contour.sourcePoints);
        raw.push_back({ i, std::move(contour) });
    }

    const double minArea = std::max(config.minAreaPx2 * scale * scale, config.minAreaMm2);

    std::unordered_map<size_t, int> srcToNew;
    for (auto& r : raw) {
        if (r.contour.area < minArea) continue;
        if (!config.printHoles && r.contour.parent >= 0) continue;
        srcToNew[r.src] = static_cast<int>(result.contours.size());
        result.contours.push_back(std::move(r.contour));
    }

    for (auto& c : result.contours) {
        if (c.parent < 0) continue;
        auto it = srcToNew.find(static_cast<size_t>(c.parent));
        c.parent = (it != srcToNew.end()) ? it->second : -1;
    }

    return result;
}
