#include "BarycentricCoordinates.h"

BarycentricCoordinates BarycentricCoordinates::fromCartesian(const Complex& point, const Complex& a, const Complex& b, const Complex& c) {
    // Calculating barycentric coordinates using areas
    Complex v0 = b - a;
    Complex v1 = c - a;
    Complex v2 = point - a;

    double d00 = v0.getReal() * v0.getReal() + v0.getImag() * v0.getImag();
    double d01 = v0.getReal() * v1.getReal() + v0.getImag() * v1.getImag();
    double d11 = v1.getReal() * v1.getReal() + v1.getImag() * v1.getImag();
    double d20 = v2.getReal() * v0.getReal() + v2.getImag() * v0.getImag();
    double d21 = v2.getReal() * v1.getReal() + v2.getImag() * v1.getImag();

    double denom = d00 * d11 - d01 * d01;

    if (std::abs(denom) < 1e-10) {
        throw std::runtime_error("Degenerate triangle in barycentric coordinates");
    }

    double beta_val = (d11 * d20 - d01 * d21) / denom;
    double gamma_val = (d00 * d21 - d01 * d20) / denom;
    double alpha_val = 1.0 - beta_val - gamma_val;

    return BarycentricCoordinates(alpha_val, beta_val, gamma_val);
}

BarycentricCoordinates BarycentricCoordinates::normalized() const {
    double sum = alpha + beta + gamma;
    if (std::abs(sum) < 1e-10) {
        throw std::runtime_error("Cannot normalize barycentric coordinates with zero sum");
    }

    return BarycentricCoordinates(alpha / sum, beta / sum, gamma / sum);
}
