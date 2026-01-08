#pragma once
#include <iostream>
#include <cmath>
#include <vector>
#include <string>

class Complex
{
private:
    double Real;
    double Imag;

public:
    Complex(double r = 0.0, double i = 0.0);

    // Arithmetic operators
    Complex operator+(const Complex& OtherNumber) const;
    Complex operator-(const Complex& OtherNumber) const;
    Complex operator*(const Complex& OtherNumber) const;
    Complex operator/(const Complex& OtherNumber) const;

    // Comparison Operators
    bool operator==(const Complex& OtherNumber) const;
    bool operator!=(const Complex& OtherNumber) const;
    bool operator>(const Complex& OtherNumber) const;
    bool operator<(const Complex& OtherNumber) const;
    bool operator>=(const Complex& OtherNumber) const;
    bool operator<=(const Complex& OtherNumber) const;

    // Assignment Operators
    Complex& operator=(const Complex& OtherNumber);
    Complex& operator+=(const Complex& OtherNumber);
    Complex& operator-=(const Complex& OtherNumber);
    Complex& operator*=(const Complex& OtherNumber);
    Complex& operator/=(const Complex& OtherNumber);

    Complex operator+();
    Complex operator-();

    Complex& operator++();
    Complex operator++(int);
    Complex& operator--();
    Complex operator--(int);

    // Access Methods
    double GetReal() const;
    double GetImag() const;
    void SetReal(double r);
    void SetImage(double i);

    // Additional Methods
    double Magnitude() const;
    double Phase() const;
    Complex Conjugate() const;
    double MagnitudeSqr() const;

    // Obtaining components in polar form
    double GetMagnitude() const;
    double GetPhase() const;
    double GetPhaseDegrees() const; // Phase in degrees

    // Setting a value using polar coordinates
    void SetPolar(double Magnitude, double Phase);
    // Phase normalization to the range [-PI / 2, PI / 2]
    void NormalizedPhase();
    // Rotation of a complex number by an angle
    Complex Rotate(double Angle) const;

    // Trigonometric functions
    Complex Sin() const;
    Complex Cos() const;
    Complex Tan() const;

    // Hyperbolic functions
    Complex Sinh() const;
    Complex Cosh() const;
    Complex Tanh() const;

    Complex Sec() const; // Secant
    Complex Csc() const; // Cosecant
    Complex Cot() const; // Cotangent

    // Exponential and logarithmic functions
    Complex Exp() const;
    Complex Log() const; // Basic logarithm
    Complex Log10() const; //  Decimal logarithm

    // Power functions
    Complex Pow(double Exponent) const;
    Complex Pow(const Complex& Exponent) const;
    Complex Sqrt() const;

    // Inverse trigonometric functions
    Complex Asin() const;
    Complex Acos() const;
    Complex Atan() const;

    Complex Asinh() const; // Area hyperbolic sine
    Complex Acosh() const; // Area hyperbolic cosine
    Complex Atanh() const; // Area hyperbolic tangent

    // Auxiliary methods
    bool IsReal() const;          // Checking for a real number
    bool IsImaginary() const;     // Testing for a purely imaginary number
    bool IsZero() const;          // Checking for zero
    bool IsFinite() const;        // Limb check
    std::string ToString() const; // String representation

    // Static factory methods
    static Complex FromPolar(double Magnitude, double Phase);
    static Complex UnitReal();
    static Complex UnitImaginary();

    // Root of the nth degree (all values)
    std::vector<Complex> Roots(int N) const;
    // Generating roots of unity
    std::vector<Complex> RootsOfUnity(int N) const;

    // Distance on a unit circle
    double DistanceTo(const Complex& OtherNumber) const;

    bool IsOnUnitCircle(double Tolerance = 1e-10) const;

    std::string ToPolarString() const;     // In polar form
    std::string ToExponentString() const;  // In exponential form

    // Static constants
    static const Complex Zero;
    static const Complex One;
    static const Complex I;    
};


// Streaming output
std::ostream& operator<<(std::ostream& Output, const Complex& ComplexNumber);