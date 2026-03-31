#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <sstream>

const double PI = 3.1415926535897932;

class OrientedAngle
{
private:

    double angle_rad; // Angle in radians

public:

    // Constructors
    OrientedAngle(double rad = 0.0) : angle_rad(rad) {}

    // Creation from degrees
    static OrientedAngle fromDegrees(double degrees);    

    // Creation from radians
    static OrientedAngle fromRadians(double radians);

    // Normalize the angle to the range [0, 2PI)
    OrientedAngle normalized() const;

    // Normalization to the range [-PI, PI)
    OrientedAngle normalizedSigned() const;    

    // Getting a value in radians
    double radians() const { return angle_rad; }

    // Getting the value in degrees
    double degrees() const { return angle_rad * 180.0 / PI; }

    // Arithmetic operations
    OrientedAngle operator+(const OrientedAngle& other) const;

    OrientedAngle operator-(const OrientedAngle& other) const;

    OrientedAngle operator*(double scalar) const;

    OrientedAngle operator/(double scalar) const;

    // Assignment Operators
    OrientedAngle& operator+=(const OrientedAngle& other);

    OrientedAngle& operator-=(const OrientedAngle& other);

    OrientedAngle& operator*=(double scalar);

    OrientedAngle& operator/=(double scalar);

    // Unary operators
    OrientedAngle operator+() const { return *this; }

    OrientedAngle operator-() const { return OrientedAngle(-angle_rad); }

    // Comparison Operators
    bool operator==(const OrientedAngle& other) const;

    bool operator!=(const OrientedAngle& other) const {
        return !(*this == other);
    }

    bool operator<(const OrientedAngle& other) const;

    bool operator<=(const OrientedAngle& other) const;

    bool operator>(const OrientedAngle& other) const;

    bool operator>=(const OrientedAngle& other) const;

    // Trigonometric functions
    double sin() const { return std::sin(angle_rad); }

    double cos() const { return std::cos(angle_rad); }

    double tan() const { return std::tan(angle_rad); }

    // Inverse trigonometric functions (static methods)
    static OrientedAngle arcsin(double value);

    static OrientedAngle arccos(double value);

    static OrientedAngle arctan(double value) {
        return OrientedAngle(std::atan(value));
    }

    static OrientedAngle arctan2(double y, double x) {
        return OrientedAngle(std::atan2(y, x));
    }

    // Special corners
    static OrientedAngle zero() {
        return OrientedAngle(0.0);
    }

    static OrientedAngle right() {
        return OrientedAngle(PI / 2.0);
    }

    static OrientedAngle straight() {
        return OrientedAngle(PI);
    }

    static OrientedAngle full() {
        return OrientedAngle(2 * PI);
    }

    // Checking special cases
    bool isZero() const;

    bool isRight() const;

    bool isStraight() const;

    bool isAcute() const;

    bool isObtuse() const;

    bool isReflex() const;

    // Additional angle (sum up to 90)
    OrientedAngle complementary() const;

    // Adjacent angle (sum up to 180)
    OrientedAngle supplementary() const;

    // String representation
    std::string toString() const {
        return std::to_string(degrees());
    }

    std::string toStringRadians() const {
        return std::to_string(angle_rad) + " rad";
    }
};