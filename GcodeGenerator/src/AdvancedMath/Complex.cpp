#include "Complex.h"

const double PI = 3.1415926535897932;

// Static constants
const Complex Complex::zero(0.0, 0.0);
const Complex Complex::one(1.0, 0.0);
const Complex Complex::i(0.0, 0.1);

Complex::Complex(double realNumber = 0.0, double imageNumber = 0.0) {
    real = realNumber;
    imag = imageNumber;
}

// Arithmetic operators
Complex Complex::operator+(const Complex& otherNumber) const {
    return Complex(real + otherNumber.real, imag + otherNumber.imag);
}

Complex Complex::operator-(const Complex& otherNumber) const {
    return Complex(real - otherNumber.real, imag - otherNumber.imag);
}

Complex Complex::operator*(const Complex& otherNumber) const {
    return Complex(real * otherNumber.real - imag * otherNumber.imag,
        real * otherNumber.imag + imag * otherNumber.real);
}

Complex Complex::operator/(const Complex& otherNumber) const {
    double realResult = 0.0;
    double imagResult = 0.0;

    if ((otherNumber.real * otherNumber.real + otherNumber.imag * otherNumber.imag) != 0) {
        realResult = (real * otherNumber.real + imag * otherNumber.imag) / (otherNumber.real * otherNumber.real + otherNumber.imag * otherNumber.imag);
        imagResult = (otherNumber.real * imag - real * otherNumber.imag) / (otherNumber.real * otherNumber.real + otherNumber.imag * otherNumber.imag);
    }

    return Complex(realResult, imagResult);
}

// Comparison Operators
bool Complex::operator==(const Complex& otherNumber) const {
    if (real == otherNumber.real && imag == otherNumber.imag) {
        return true;
    }
    else {
        return false;
    }
}

bool Complex::operator!=(const Complex& otherNumber) const {
    if (real != otherNumber.real || imag != otherNumber.imag) {
        return true;
    }
    else {
        return false;
    }
}

bool Complex::operator>(const Complex& otherNumber) const {
    return this->magnitudeSqr() > otherNumber.magnitudeSqr();
}

bool Complex::operator<(const Complex& otherNumber) const {
    return this->magnitudeSqr() < otherNumber.magnitudeSqr();
}

bool Complex::operator>=(const Complex& otherNumber) const {
    return this->magnitudeSqr() >= otherNumber.magnitudeSqr();
}

bool Complex::operator<=(const Complex& otherNumber) const {
    return this->magnitudeSqr() <= otherNumber.magnitudeSqr();
}

// Assignment Operators
Complex& Complex::operator=(const Complex& otherNumber) {
    if (this != &otherNumber) {
        real = otherNumber.real;
        imag = otherNumber.imag;
    }

    return *this;
}

Complex& Complex::operator+=(const Complex& otherNumber) {
    real += otherNumber.real;
    imag += otherNumber.imag;

    return *this;
}

Complex& Complex::operator-=(const Complex& otherNumber) {
    real -= otherNumber.real;
    imag -= otherNumber.imag;

    return *this;
}

Complex& Complex::operator*=(const Complex& otherNumber) {
    double newReal = real * otherNumber.real - imag * otherNumber.imag;
    double newImag = real * otherNumber.imag + imag * otherNumber.real;
    real = newReal;
    imag = newImag;

    return *this;
}

Complex& Complex::operator/=(const Complex& otherNumber) {
    if (otherNumber != 0) {
        *this = *this / otherNumber;
    }

    return *this;
}

// Unary operators
Complex Complex::operator+() {
    return *this;
}

Complex Complex::operator-() {
    return Complex(-real, -imag);
}

// Increment/decrement operators
Complex& Complex::operator++() { // Prefix
    ++real;
    return *this;
}

Complex Complex::operator++(int) { // Postfix
    Complex temp = *this;
    ++real;
    return temp;
}

Complex& Complex::operator--() { // Prefix
    --real;
    return *this;
}

Complex Complex::operator--(int) { // Postfix
    Complex temp = *this;
    --real;
    return temp;
}

// Access Methods
double Complex::getReal() const {
    return real;
}

double Complex::getImag() const {
    return imag;
}

void Complex::setReal(double realNumber) {
    real = realNumber;
}

void Complex::setImage(double imageNumber) {
    imag = imageNumber;
}

// Basic mathematical operations
double Complex::magnitude() const {
    return std::sqrt(real * real + imag * imag);
}

double Complex::phase() const {
    return std::atan2(imag, real);
}

Complex Complex::conjugate() const {
    return Complex(real, -imag);
}

double Complex::magnitudeSqr() const {
    return real * real + imag * imag;
}

// Obtaining components in polar form
double Complex::getMagnitude() const {
    return magnitude();
}

double Complex::getPhase() const {
    return phase();
}

// Phase in degrees
double Complex::getPhaseDegrees() const {
    return phase() * 180.0 / PI;
}

// Setting a value using polar coordinates
void Complex::setPolar(double magnitude, double phase) {
    real = magnitude * std::cos(phase);
    imag = magnitude * std::sin(phase);
}

void Complex::normalizedPhase() {
    double mag = magnitude();
    double phaseValue = phase();

    // Phase normalization to the range [-PI/2, PI/2]
    while (phaseValue > PI) {
        phaseValue -= 2 * PI;
    }
    while (phaseValue <= PI) {
        phaseValue += 2 * PI;
    }

    setPolar(mag, phaseValue);
}

// Rotation of a complex number by an angle
Complex Complex::rotate(double angle) const {
    return *this * fromPolar(1.0, angle);
}

// Trigonometric functions
Complex Complex::sin() const {
    return Complex(std::sin(real) * std::cosh(imag),
        std::cos(real) * std::sinh(imag));
}

Complex Complex::cos() const {
    return Complex(std::cos(real) * std::cosh(imag), 
        -std::sin(real) * std::sinh(imag));
}

Complex Complex::tan() const {
    Complex cosValue = cos();
    if (cosValue != zero) {
        return sin() / cosValue;
    }
    else {
        return Complex(0.0, 0.0);
    }
}

// Hyperbolic functions
Complex Complex::sinh() const {
    return Complex(std::sinh(real) * std::cos(imag),
        std::cosh(real) * std::sin(imag));
}

Complex Complex::cosh() const {
    return Complex(std::cosh(real) * std::cos(imag),
        std::sinh(real) * std::sin(imag));
}

Complex Complex::tanh() const {
    Complex coshValue = cosh();
    if (coshValue != zero) {
        return sinh() / coshValue;
    }
    else {
        return Complex(0.0, 0.0);
    }
}

// Secant
Complex Complex::sec() const {
    Complex cosValue = cos();
    if (cosValue != zero) {
        return one / cosValue;
    }
    else {
        return Complex(0.0, 0.0);
    }
}

// Cosecant
Complex Complex::csc() const {
    Complex sinValue = sin();
    if (sinValue != zero) {
        return one / sinValue;
    }
    else {
        return Complex(0.0, 0.0);
    }
}

// Cotangent
Complex Complex::cot() const {
    Complex tanValue = tan();
    if (tanValue != zero) {
        return one / tanValue;
    }
    else {
        return Complex(0.0, 0.0);
    }
}

// Exponential and logarithmic functions
Complex Complex::exp() const {
    double expReal = std::exp(real);
    return Complex(expReal * std::cos(imag), expReal * std::sin(imag));
}

Complex Complex::log() const {
    return Complex(std::log(this->magnitude()), this->phase());
}

Complex Complex::log10() const {
    return this->log() / std::log(10.0);
}

// Power functions
Complex Complex::pow(double exponent) const {
    double mag = std::pow(this->magnitude(), exponent);
    double phaseValue = this->phase() * exponent;

    return Complex(mag * std::cos(phaseValue), mag * std::sin(phaseValue));
}

Complex Complex::pow(const Complex& exponent) const {
    return (exponent * this->log()).exp();
}

Complex Complex::sqrt() const {
    return this->pow(0.5);
}

// Inverse trigonometric functions
Complex Complex::asin() const {
    Complex i(0, 1);
    return -i * ((*this * i + (one - this->pow(2)).sqrt()).log());
}

Complex Complex::acos() const {
    Complex i(0, 1);
    return (i * ((*this * i + (one - this->pow(2)).sqrt()).log()) - (PI / 2));
}

Complex Complex::atan() const {
    Complex i(0, 1);
    if (i - *this != 0) {
        return (i / 2) * ((i + *this) / (i - *this)).log();
    }
    else {
        return Complex(0.0, 0.0);
    }
}

// Area hyperbolic sine
Complex Complex::asinh() const {
    return ((*this) + ((*this) * (*this) + one).sqrt()).log();
}

// Area hyperbolic cosine
Complex Complex::acosh() const {
    return ((*this) + ((*this) * (*this) - one).sqrt()).log();
}

// Area hyperbolic tangent
Complex Complex::atanh() const {
    if (*this == one || *this == -1 * one) {
        return Complex(0.0, 0.0);
    }
    else {
        return (0.5) * ((one + (*this)) / (one - (*this))).log();
    }
}

// =====Helper Methods=====
// Checking for a real number
bool Complex::isReal() const {
    return imag == 0.0;
}

// Testing for a purely imaginary number
bool Complex::isImaginary() const {
    return real == 0.0 && imag != 0.0;
}

// Checking for zero
bool Complex::isZero() const {
    return real == 0.0 && imag == 0.0;
}

// Limb check
bool Complex::isFinite() const {
    return std::isfinite(real) && std::isfinite(imag);
}

// String representation
std::string Complex::toString() const {
    if (isZero()) {
        return "0";
    }

    std::string result;
    if (real != 0.0) {
        result += std::to_string(real);
    }

    if (imag != 0.0) {
        if (imag > 0.0 && real != 0.0) {
            result += "+";
        }
        if (imag == 1.0) {
            result += "i";
        }
        else if (imag == -1.0) {
            result += "-i";
        }
        else {
            result += std::to_string(imag) + "i";
        }
    }

    return result;
}

// Static factory methods
Complex Complex::fromPolar(double magnitude, double phase) {
    return Complex(magnitude * std::cos(phase), magnitude * std::sin(phase));
}

Complex Complex::unitReal() {
    return one;
}

Complex Complex::unitImaginary() {
    return i;
}

// Root of the nth degree (all values)
std::vector<Complex> Complex::roots(int number) const {
    if (number <= 0) {
        return {};
    }

    std::vector<Complex> roots;
    double mag = std::pow(magnitude(), 1.0 / number);
    double phaseValue = phase();

    double rootPhase;
    for (int k = 0; k < number; k++) {
        rootPhase = (phaseValue + 2 * PI * k) / number;
        roots.push_back(fromPolar(mag, rootPhase));
    }

    return roots;
}

// Generating roots of unity
std::vector<Complex> Complex::rootsOfUnity(int number) const {
    return Complex::one.roots(number);
}

// Distance on a unit circle
double Complex::distanceTo(const Complex& otherNumber) const {
    return (*this - otherNumber).magnitude();
}

bool Complex::isOnUnitCircle(double tolerance) const {
    return std::abs(magnitudeSqr() - 1.0) < tolerance;
}

// In polar form
std::string Complex::toPolarString() const {
    double mag = magnitude();
    double phaseValue = phase();
    return std::to_string(mag) + " angle " + std::to_string(phaseValue);
}

// In exponential form
std::string Complex::toExponentString() const {
    double mag = magnitude();
    double phaseValue = phase();
    return std::to_string(mag) + "e^(i" + std::to_string(phaseValue) + ")";
}

// External operators for working with doubles
Complex operator+(double ihs, const Complex rhs) {
    return Complex(ihs) + rhs;
}

Complex operator-(double ihs, const Complex rhs) {
    return Complex(ihs) - rhs;
}

Complex operator*(double ihs, const Complex rhs) {
    return Complex(ihs) * rhs;
}

Complex operator/(double ihs, const Complex rhs) {
    if (rhs != 0) {
        return Complex(ihs) / rhs;
    }
    else {
        return Complex(0.0, 0.0);
    }
}

// Streaming output
std::ostream& operator<<(std::ostream& output, const Complex& complexNumber) {
    output << complexNumber.toString();
    return output;
}
