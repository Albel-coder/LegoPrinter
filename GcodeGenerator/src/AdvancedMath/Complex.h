#pragma once
#include <iostream>
#include <cmath>
#include <vector>
#include <string>

class Complex
{
private:
    double real;
    double imag;

public:
    Complex(double realNumber = 0.0, double imageNumber = 0.0);

    // Arithmetic operators
    Complex operator+(const Complex& otherNumber) const;
    Complex operator-(const Complex& otherNumber) const;
    Complex operator*(const Complex& otherNumber) const;
    Complex operator/(const Complex& otherNumber) const;

    // Comparison Operators
    bool operator==(const Complex& otherNumber) const;
    bool operator!=(const Complex& otherNumber) const;
    bool operator>(const Complex& otherNumber) const;
    bool operator<(const Complex& otherNumber) const;
    bool operator>=(const Complex& otherNumber) const;
    bool operator<=(const Complex& otherNumber) const;

    // Assignment Operators
    Complex& operator=(const Complex& otherNumber);
    Complex& operator+=(const Complex& otherNumber);
    Complex& operator-=(const Complex& otherNumber);
    Complex& operator*=(const Complex& otherNumber);
    Complex& operator/=(const Complex& otherNumber);

    Complex operator+();
    Complex operator-();

    Complex& operator++();
    Complex operator++(int);
    Complex& operator--();
    Complex operator--(int);

    // Access Methods
    double getReal() const;
    double getImag() const;
    void setReal(double realNumber);
    void setImage(double imageNumber);

    // Additional Methods
    double magnitude() const;
    double phase() const;
    Complex conjugate() const;
    double magnitudeSqr() const;

    // Obtaining components in polar form
    double getMagnitude() const;
    double getPhase() const;
    double getPhaseDegrees() const; // Phase in degrees

    // Setting a value using polar coordinates
    void setPolar(double magnitude, double phase);
    // Phase normalization to the range [-PI / 2, PI / 2]
    void normalizedPhase();
    // Rotation of a complex number by an angle
    Complex rotate(double angle) const;

    // Trigonometric functions
    Complex sin() const;
    Complex cos() const;
    Complex tan() const;

    // Hyperbolic functions
    Complex sinh() const;
    Complex cosh() const;
    Complex tanh() const;

    Complex sec() const; // Secant
    Complex csc() const; // Cosecant
    Complex cot() const; // Cotangent

    // Exponential and logarithmic functions
    Complex exp() const;
    Complex log() const; // Basic logarithm
    Complex log10() const; //  Decimal logarithm

    // Power functions
    Complex pow(double exponent) const;
    Complex pow(const Complex& exponent) const;
    Complex sqrt() const;

    // Inverse trigonometric functions
    Complex asin() const;
    Complex acos() const;
    Complex atan() const;

    Complex asinh() const; // Area hyperbolic sine
    Complex acosh() const; // Area hyperbolic cosine
    Complex atanh() const; // Area hyperbolic tangent

    // Auxiliary methods
    bool isReal() const;          // Checking for a real number
    bool isImaginary() const;     // Testing for a purely imaginary number
    bool isZero() const;          // Checking for zero
    bool isFinite() const;        // Limb check
    std::string toString() const; // String representation

    // Static factory methods
    static Complex fromPolar(double magnitude, double phase);
    static Complex unitReal();
    static Complex unitImaginary();

    // Root of the nth degree (all values)
    std::vector<Complex> roots(int number) const;
    // Generating roots of unity
    std::vector<Complex> rootsOfUnity(int nember) const;

    // Distance on a unit circle
    double distanceTo(const Complex& otherNumber) const;

    bool isOnUnitCircle(double tolerance = 1e-10) const;

    std::string toPolarString() const;     // In polar form
    std::string toExponentString() const;  // In exponential form

    // Static constants
    static const Complex zero;
    static const Complex one;
    static const Complex i;    
};


// Streaming output
std::ostream& operator<<(std::ostream& output, const Complex& complexNumber);