#include "Complex.h"

const double PI = 3.1415926535897932;

class BarycentricCoordinates
{

private:

    double alpha, beta, gamma; // Barycentric Coordinates (?, ?, ?)

public:

    // Constructors
    BarycentricCoordinates(double a = 0.0, double b = 0.0, double c = 0.0)
        : alpha(a), beta(b), gamma(c) {
    }

    // Creation from Cartesian coordinates relative to a triangle
    static BarycentricCoordinates fromCartesian(const Complex& point, const Complex& a, const Complex& b, const Complex& c);

    // Creating special points of a triangle
    static BarycentricCoordinates vertexA() {
        return BarycentricCoordinates(1.0, 0.0, 0.0);
    }

    static BarycentricCoordinates vertexB() {
        return BarycentricCoordinates(0.0, 1.0, 0.0);
    }

    static BarycentricCoordinates vertexC() {
        return BarycentricCoordinates(0.0, 0.0, 1.0);
    }

    static BarycentricCoordinates centroid() {
        return BarycentricCoordinates(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);
    }

    static BarycentricCoordinates incenter() {
        // All coordinates are equal for an equilateral triangle
        return centroid();
    }

    // Getters
    double getAlpha() const { return alpha; }
    double getBeta() const { return beta; }
    double getGamma() const { return gamma; }

    // Setters
    void setAlpha(double a) { alpha = a; }
    void setBeta(double b) { beta = b; }
    void setGamma(double g) { gamma = g; }

    // Normalization (sum of coordinates = 1)
    BarycentricCoordinates normalized() const;

    // Validation check
    bool isValid() const {
        return alpha >= 0.0 && beta >= 0.0 && gamma >= 0.0;
    }

    bool isInsideTriangle() const {
        return isValid() && std::abs(alpha + beta + gamma - 1.0) < 1e-10;
    }

    bool isOnEdge() const {
        return isValid() && (std::abs(alpha) < 1e-10 ||
            std::abs(beta) < 1e-10 ||
            std::abs(gamma) < 1e-10);
    }

    bool isOnVertex() const {
        return (std::abs(alpha - 1.0) < 1e-10 && std::abs(beta) < 1e-10 && std::abs(gamma) < 1e-10) ||
            (std::abs(beta - 1.0) < 1e-10 && std::abs(alpha) < 1e-10 && std::abs(gamma) < 1e-10) ||
            (std::abs(gamma - 1.0) < 1e-10 && std::abs(alpha) < 1e-10 && std::abs(beta) < 1e-10);
    }

    // Transformation to Cartesian coordinates{
    Complex toCartesian(const Complex& a, const Complex& b, const Complex& c) const {
        return a * alpha + b * beta + c * gamma;
    }

    // Arithmetic operations
    BarycentricCoordinates operator+(const BarycentricCoordinates& other) const {
        return BarycentricCoordinates(alpha + other.alpha,
            beta + other.beta,
            gamma + other.gamma);
    }

    BarycentricCoordinates operator-(const BarycentricCoordinates& other) const {
        return BarycentricCoordinates(alpha - other.alpha,
            beta - other.beta,
            gamma - other.gamma);
    }

    BarycentricCoordinates operator*(double scalar) const {
        return BarycentricCoordinates(alpha * scalar,
            beta * scalar,
            gamma * scalar);
    }

    BarycentricCoordinates operator/(double scalar) const {
        if (std::abs(scalar) < 1e-10) {
            throw std::runtime_error("Division by zero in barycentric coordinates");
        }

        return BarycentricCoordinates(alpha / scalar,
            beta / scalar,
            gamma / scalar);
    }

    // Assignment Operators
    BarycentricCoordinates& operator+=(const BarycentricCoordinates& other) {
        alpha += other.alpha;
        beta += other.beta;
        gamma += other.gamma;

        return *this;
    }

    BarycentricCoordinates& operator-=(const BarycentricCoordinates& other) {
        alpha -= other.alpha;
        beta -= other.beta;
        gamma -= other.gamma;

        return *this;
    }

    BarycentricCoordinates& operator*=(double scalar) {
        alpha *= scalar;
        beta *= scalar;
        gamma *= scalar;

        return *this;
    }

    BarycentricCoordinates& operator/=(double scalar) {
        if (std::abs(scalar) < 1e-10) {
            throw std::runtime_error("Division by zero in barycentric coordinates");
        }
        alpha /= scalar;
        beta /= scalar;
        gamma /= scalar;

        return *this;
    }

    // Interpolation of values ??at vertices
    template<typename T>
    static T interpolate(const BarycentricCoordinates& coords, const T& valueA, const T& valueB, const T& valueC) {
        return valueA * coords.Alpha + valueB * coords.Beta + valueC * coords.Gamma;
    }

    // Color interpolation (vector specialization)
    static std::vector<double> interpolateColor(const BarycentricCoordinates& coords,
        const std::vector<double>& colorA,
        const std::vector<double>& colorB,
        const std::vector<double>& colorC) {

        if (colorA.size() != colorB.size() || colorA.size() != colorC.size()) {
            throw std::runtime_error("Color vectors must have same size");
        }

        std::vector<double> result(colorA.size());
        for (size_t i = 0; i < colorA.size(); ++i) {
            result[i] = coords.alpha * colorA[i] + coords.beta * colorB[i] + coords.gamma * colorC[i];
        }

        return result;
    }

    // Distance between barycentric coordinates
    double distanceTo(const BarycentricCoordinates& other) const {
        double da = alpha - other.alpha;
        double db = beta - other.beta;
        double dg = gamma - other.gamma;

        return std::sqrt(da * da + db * db + dg * dg);
    }

    // Linear interpolation between two barycentric coordinates
    static BarycentricCoordinates lerp(const BarycentricCoordinates& a,
        const BarycentricCoordinates& b, double t) {
        return a * (1.0 - t) + b * t;
    }

    // Comparison Operators
    bool operator==(const BarycentricCoordinates& other) const {
        return std::abs(alpha - other.alpha) < 1e-10 &&
            std::abs(beta - other.beta) < 1e-10 &&
            std::abs(gamma - other.gamma) < 1e-10;
    }

    bool operator!=(const BarycentricCoordinates& other) const {
        return !(*this == other);
    }

    // String representation
    std::string toString() const {
        return "Barycentric(" +
            std::to_string(alpha) + ", " +
            std::to_string(beta) + ", " +
            std::to_string(gamma) + ")";
    }

    // Checking special provisions
    bool isCentroid() const {
        auto norm = this->normalized();
        return std::abs(norm.alpha - 1.0 / 3.0) < 1e-10 &&
            std::abs(norm.beta - 1.0 / 3.0) < 1e-10 &&
            std::abs(norm.gamma - 1.0 / 3.0) < 1e-10;
    }
};

// Inference operator
std::ostream& operator<<(std::ostream& os, const BarycentricCoordinates& bc) {
    os << bc.toString();
    return os;
}

// Multiplication by a scalar (commutativity)
BarycentricCoordinates operator*(double scalar, const BarycentricCoordinates& bc) {
    return bc * scalar;
}