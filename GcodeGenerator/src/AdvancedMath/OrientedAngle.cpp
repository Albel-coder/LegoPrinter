#include "OrientedAngle.h"

OrientedAngle OrientedAngle::fromDegrees(double degrees) {
    return OrientedAngle(degrees * PI / 180.0);
}

OrientedAngle OrientedAngle::fromRadians(double radians) {
    return OrientedAngle(radians);
}

OrientedAngle OrientedAngle::normalized() const {
    double normalized = std::fmod(angle_rad, 2 * PI);
    if (normalized < 0) {
        normalized += 2 * PI;
    }

    return OrientedAngle(normalized);
}

OrientedAngle OrientedAngle::normalizedSigned() const {
    double normalized = std::fmod(angle_rad, 2 * PI);
    if (normalized > PI) {
        normalized -= 2 * PI;
    }
    else if (normalized <= -PI) {
        normalized += 2 * PI;
    }

    return OrientedAngle(normalized);
}

OrientedAngle OrientedAngle::operator+(const OrientedAngle& other) const {
    return OrientedAngle(angle_rad + other.angle_rad);
}

OrientedAngle OrientedAngle::operator-(const OrientedAngle& other) const {
    return OrientedAngle(angle_rad - other.angle_rad);
}

OrientedAngle OrientedAngle::operator*(double scalar) const {
    return OrientedAngle(angle_rad * scalar);
}

OrientedAngle OrientedAngle::operator/(double scalar) const {
    if (scalar == 0.0) {
        throw std::runtime_error("Division by zero");
    }

    return OrientedAngle(angle_rad / scalar);
}

OrientedAngle& OrientedAngle::operator+=(const OrientedAngle& other) {
    angle_rad += other.angle_rad;
    return *this;
}

OrientedAngle& OrientedAngle::operator-=(const OrientedAngle& other) {
    angle_rad -= other.angle_rad;
    return *this;
}

OrientedAngle& OrientedAngle::operator*=(double scalar) {
    angle_rad *= scalar;
    return *this;
}

OrientedAngle& OrientedAngle::operator/=(double scalar) {
    if (scalar == 0.0) {
        throw std::runtime_error("Division by zero");
    }
    angle_rad /= scalar;

    return *this;
}

bool OrientedAngle::operator==(const OrientedAngle& other) const {
    auto norm1 = this->normalized();
    auto norm2 = other.normalized();

    return std::abs(norm1.angle_rad - norm2.angle_rad) < 1e-10;
}

bool OrientedAngle::operator<(const OrientedAngle& other) const {
    auto norm1 = this->normalized();
    auto norm2 = other.normalized();

    return norm1.angle_rad < norm2.angle_rad;
}

bool OrientedAngle::operator<=(const OrientedAngle& other) const
{
    auto norm1 = this->normalized();
    auto norm2 = other.normalized();

    return norm1.angle_rad <= norm2.angle_rad;
}

bool OrientedAngle::operator>(const OrientedAngle& other) const {
    auto norm1 = this->normalized();
    auto norm2 = other.normalized();

    return norm1.angle_rad > norm2.angle_rad;
}

bool OrientedAngle::operator>=(const OrientedAngle& other) const {
    auto norm1 = this->normalized();
    auto norm2 = other.normalized();

    return norm1.angle_rad >= norm2.angle_rad;
}

OrientedAngle OrientedAngle::arcsin(double value) {
    if (value < -1.0 || value > 1.0) {
        throw std::runtime_error("Value out of range for arcsin");
    }

    return OrientedAngle(std::asin(value));
}

OrientedAngle OrientedAngle::arccos(double value) {
    if (value < -1.0 || value > 1.0) {
        throw std::runtime_error("Value out of range for arccos");
    }

    return OrientedAngle(std::acos(value));
}

bool OrientedAngle::isZero() const {
    auto norm = this->normalized();
    return std::abs(norm.angle_rad) < 1e-10;
}

bool OrientedAngle::isRight() const {
    auto norm = this->normalized();
    return std::abs(norm.angle_rad - PI / 2.0) < 1e-10;
}

bool OrientedAngle::isStraight() const {
    auto norm = this->normalized();
    return std::abs(norm.angle_rad - PI) < 1e-10;
}

bool OrientedAngle::isAcute() const {
    auto norm = this->normalized();
    return norm.angle_rad > 0 && norm.angle_rad < PI / 2.0;
}

bool OrientedAngle::isObtuse() const {
    auto norm = this->normalized();
    return norm.angle_rad > PI / 2.0 && norm.angle_rad < PI;
}

bool OrientedAngle::isReflex() const {
    auto norm = this->normalized();
    return norm.angle_rad > PI && norm.angle_rad < 2 * PI;
}

OrientedAngle OrientedAngle::complementary() const {
    auto norm = this->normalized();
    if (norm.angle_rad > PI / 2.0) {
        throw std::runtime_error("Angle too large for complementary angle");
    }

    return OrientedAngle(PI / 2.0 - norm.angle_rad);
}

OrientedAngle OrientedAngle::supplementary() const {
    auto norm = this->normalized();
    if (norm.angle_rad > PI) {
        throw std::runtime_error("Angle too large for supplementary angle");
    }

    return OrientedAngle(PI - norm.angle_rad);
}


// Inference operator
std::ostream& operator<<(std::ostream& os, const OrientedAngle& angle) {
    os << angle.toString();
    return os;
}

// Multiplication by a scalar (commutativity)
OrientedAngle operator*(double scalar, const OrientedAngle& angle) {
    return angle * scalar;
}