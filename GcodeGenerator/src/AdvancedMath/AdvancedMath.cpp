#include "Complex.h"

const double PI = 3.1415926535897932;

class BarycentricCoordinates
{

private:

    double Alpha, Beta, Gamma; // Barycentric Coordinates (α, β, γ)

public:

    // Constructors
    BarycentricCoordinates(double a = 0.0, double b = 0.0, double c = 0.0)
        : Alpha(a), Beta(b), Gamma(c) {
    }

    // Creation from Cartesian coordinates relative to a triangle
    static BarycentricCoordinates fromCartesian(const Complex& point,
        const Complex& A,
        const Complex& B,
        const Complex& C)
    {
        // Calculating barycentric coordinates using areas
        Complex v0 = B - A;
        Complex v1 = C - A;
        Complex v2 = point - A;

        double d00 = v0.GetReal() * v0.GetReal() + v0.GetImag() * v0.GetImag();
        double d01 = v0.GetReal() * v1.GetReal() + v0.GetImag() * v1.GetImag();
        double d11 = v1.GetReal() * v1.GetReal() + v1.GetImag() * v1.GetImag();
        double d20 = v2.GetReal() * v0.GetReal() + v2.GetImag() * v0.GetImag();
        double d21 = v2.GetReal() * v1.GetReal() + v2.GetImag() * v1.GetImag();

        double denom = d00 * d11 - d01 * d01;

        if (std::abs(denom) < 1e-10)
        {
            throw std::runtime_error("Degenerate triangle in barycentric coordinates");
        }

        double beta_val = (d11 * d20 - d01 * d21) / denom;
        double gamma_val = (d00 * d21 - d01 * d20) / denom;
        double alpha_val = 1.0 - beta_val - gamma_val;

        return BarycentricCoordinates(alpha_val, beta_val, gamma_val);
    }

    // Creating special points of a triangle
    static BarycentricCoordinates vertexA()
    {
        return BarycentricCoordinates(1.0, 0.0, 0.0);
    }

    static BarycentricCoordinates vertexB()
    {
        return BarycentricCoordinates(0.0, 1.0, 0.0);
    }

    static BarycentricCoordinates vertexC()
    {
        return BarycentricCoordinates(0.0, 0.0, 1.0);
    }

    static BarycentricCoordinates centroid()
    {
        return BarycentricCoordinates(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);
    }

    static BarycentricCoordinates incenter()
    {
        // All coordinates are equal for an equilateral triangle
        return centroid();
    }

    // Getters
    double getAlpha() const { return Alpha; }
    double getBeta() const { return Beta; }
    double getGamma() const { return Gamma; }

    // Setters
    void setAlpha(double a) { Alpha = a; }
    void setBeta(double b) { Beta = b; }
    void setGamma(double g) { Gamma = g; }

    // Normalization (sum of coordinates = 1)
    BarycentricCoordinates normalized() const
    {
        double sum = Alpha + Beta + Gamma;
        if (std::abs(sum) < 1e-10)
        {
            throw std::runtime_error("Cannot normalize barycentric coordinates with zero sum");
        }
        return BarycentricCoordinates(Alpha / sum, Beta / sum, Gamma / sum);
    }

    // Validation check
    bool isValid() const
    {
        return Alpha >= 0.0 && Beta >= 0.0 && Gamma >= 0.0;
    }

    bool isInsideTriangle() const
    {
        return isValid() && std::abs(Alpha + Beta + Gamma - 1.0) < 1e-10;
    }

    bool isOnEdge() const
    {
        return isValid() && (std::abs(Alpha) < 1e-10 ||
            std::abs(Beta) < 1e-10 ||
            std::abs(Gamma) < 1e-10);
    }

    bool isOnVertex() const
    {
        return (std::abs(Alpha - 1.0) < 1e-10 && std::abs(Beta) < 1e-10 && std::abs(Gamma) < 1e-10) ||
            (std::abs(Beta - 1.0) < 1e-10 && std::abs(Alpha) < 1e-10 && std::abs(Gamma) < 1e-10) ||
            (std::abs(Gamma - 1.0) < 1e-10 && std::abs(Alpha) < 1e-10 && std::abs(Beta) < 1e-10);
    }

    // Transformation to Cartesian coordinates{
    Complex toCartesian(const Complex& A, const Complex& B, const Complex& C) const
    {
        return A * Alpha + B * Beta + C * Gamma;
    }

    // Arithmetic operations
    BarycentricCoordinates operator+(const BarycentricCoordinates& other) const
    {
        return BarycentricCoordinates(Alpha + other.Alpha,
            Beta + other.Beta,
            Gamma + other.Gamma);
    }

    BarycentricCoordinates operator-(const BarycentricCoordinates& other) const
    {
        return BarycentricCoordinates(Alpha - other.Alpha,
            Beta - other.Beta,
            Gamma - other.Gamma);
    }

    BarycentricCoordinates operator*(double scalar) const {
        return BarycentricCoordinates(Alpha * scalar,
            Beta * scalar,
            Gamma * scalar);
    }

    BarycentricCoordinates operator/(double scalar) const
    {
        if (std::abs(scalar) < 1e-10)
        {
            throw std::runtime_error("Division by zero in barycentric coordinates");
        }
        return BarycentricCoordinates(Alpha / scalar,
            Beta / scalar,
            Gamma / scalar);
    }

    // Assignment Operators
    BarycentricCoordinates& operator+=(const BarycentricCoordinates& other)
    {
        Alpha += other.Alpha;
        Beta += other.Beta;
        Gamma += other.Gamma;
        return *this;
    }

    BarycentricCoordinates& operator-=(const BarycentricCoordinates& other)
    {
        Alpha -= other.Alpha;
        Beta -= other.Beta;
        Gamma -= other.Gamma;
        return *this;
    }

    BarycentricCoordinates& operator*=(double scalar)
    {
        Alpha *= scalar;
        Beta *= scalar;
        Gamma *= scalar;
        return *this;
    }

    BarycentricCoordinates& operator/=(double scalar)
    {
        if (std::abs(scalar) < 1e-10)
        {
            throw std::runtime_error("Division by zero in barycentric coordinates");
        }
        Alpha /= scalar;
        Beta /= scalar;
        Gamma /= scalar;
        return *this;
    }

    // Interpolation of values ​​at vertices
    template<typename T>
    static T interpolate(const BarycentricCoordinates& coords,
        const T& valueA,
        const T& valueB,
        const T& valueC)
    {
        return valueA * coords.Alpha + valueB * coords.Beta + valueC * coords.Gamma;
    }

    // Color interpolation (vector specialization)
    static std::vector<double> interpolateColor(const BarycentricCoordinates& coords,
        const std::vector<double>& colorA,
        const std::vector<double>& colorB,
        const std::vector<double>& colorC)
    {
        if (colorA.size() != colorB.size() || colorA.size() != colorC.size())
        {
            throw std::runtime_error("Color vectors must have same size");
        }

        std::vector<double> result(colorA.size());
        for (size_t i = 0; i < colorA.size(); ++i)
        {
            result[i] = coords.Alpha * colorA[i] + coords.Beta * colorB[i] + coords.Gamma * colorC[i];
        }
        return result;
    }

    // Distance between barycentric coordinates
    double distanceTo(const BarycentricCoordinates& other) const
    {
        double da = Alpha - other.Alpha;
        double db = Beta - other.Beta;
        double dg = Gamma - other.Gamma;
        return std::sqrt(da * da + db * db + dg * dg);
    }

    // Linear interpolation between two barycentric coordinates
    static BarycentricCoordinates lerp(const BarycentricCoordinates& a,
        const BarycentricCoordinates& b, double t) {
        return a * (1.0 - t) + b * t;
    }

    // Comparison Operators
    bool operator==(const BarycentricCoordinates& other) const
    {
        return std::abs(Alpha - other.Alpha) < 1e-10 &&
            std::abs(Beta - other.Beta) < 1e-10 &&
            std::abs(Gamma - other.Gamma) < 1e-10;
    }

    bool operator!=(const BarycentricCoordinates& other) const
    {
        return !(*this == other);
    }

    // String representation
    std::string toString() const
    {
        return "Barycentric(" +
            std::to_string(Alpha) + ", " +
            std::to_string(Beta) + ", " +
            std::to_string(Gamma) + ")";
    }

    // Checking special provisions
    bool isCentroid() const
    {
        auto norm = this->normalized();
        return std::abs(norm.Alpha - 1.0 / 3.0) < 1e-10 &&
            std::abs(norm.Beta - 1.0 / 3.0) < 1e-10 &&
            std::abs(norm.Gamma - 1.0 / 3.0) < 1e-10;
    }
};

// Inference operator
std::ostream& operator<<(std::ostream& os, const BarycentricCoordinates& bc)
{
    os << bc.toString();
    return os;
}

// Multiplication by a scalar (commutativity)
BarycentricCoordinates operator*(double scalar, const BarycentricCoordinates& bc)
{
    return bc * scalar;
}

int main()
{    
    // Testing barycentric coordinates
    std::cout << "\n=== Testing Barycentric Coordinates ===" << "\n";

    // Vertices of a triangle
    Complex A(0.0, 0.0);
    Complex B(4.0, 0.0);
    Complex C(2.0, 3.0);

    std::cout << "Triangle vertices: A=" << A << ", B=" << B << ", C=" << C << "\n";

    // Special points
    BarycentricCoordinates centroid = BarycentricCoordinates::centroid();
    BarycentricCoordinates vertexA = BarycentricCoordinates::vertexA();
    BarycentricCoordinates vertexB = BarycentricCoordinates::vertexB();

    std::cout << "Centroid coords: " << centroid << "\n";
    std::cout << "Vertex A coords: " << vertexA << "\n";
    std::cout << "Vertex B coords: " << vertexB << "\n";

    // Transformation to Cartesian coordinates
    Complex centroid_point = centroid.toCartesian(A, B, C);
    Complex vertexA_point = vertexA.toCartesian(A, B, C);

    std::cout << "Centroid point: " << centroid_point << "\n";
    std::cout << "Vertex A point: " << vertexA_point << "\n";

    // Creation from Cartesian coordinates
    Complex test_point(2.0, 1.0);
    BarycentricCoordinates test_coords = BarycentricCoordinates::fromCartesian(test_point, A, B, C);

    std::cout << "Point " << test_point << " has barycentric coords: " << test_coords << "\n";
    std::cout << "Is inside triangle? " << (test_coords.isInsideTriangle() ? "Yes" : "No") << "\n";

    // Interpolation of values
    double valueA = 10.0, valueB = 20.0, valueC = 30.0;
    double interpolated_value = BarycentricCoordinates::interpolate(test_coords, valueA, valueB, valueC);

    std::cout << "Interpolated value: " << interpolated_value << "\n";

    // Color interpolation (RGB)
    std::vector<double> colorA = { 1.0, 0.0, 0.0 }; // Red
    std::vector<double> colorB = { 0.0, 1.0, 0.0 }; // Green
    std::vector<double> colorC = { 0.0, 0.0, 1.0 }; // Blue

    std::vector<double> interpolated_color = BarycentricCoordinates::interpolateColor(
        test_coords, colorA, colorB, colorC);

    std::cout << "Interpolated color: ("
        << interpolated_color[0] << ", "
        << interpolated_color[1] << ", "
        << interpolated_color[2] << ")\n";

    // Linear interpolation between barycentric coordinates
    BarycentricCoordinates lerp_result = BarycentricCoordinates::lerp(vertexA, centroid, 0.5);
    std::cout << "Lerp between A and centroid: " << lerp_result << "\n";

    // Checking provisions
    std::cout << "Is centroid? " << (centroid.isCentroid() ? "Yes" : "No") << "\n";
    std::cout << "Is on vertex? " << (vertexA.isOnVertex() ? "Yes" : "No") << "\n";

    return 0;
}