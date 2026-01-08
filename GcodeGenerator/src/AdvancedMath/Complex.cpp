#include "Complex.h"

const double PI = 3.1415926535897932;

// Static constants
const Complex Complex::Zero(0.0, 0.0);
const Complex Complex::One(1.0, 0.0);
const Complex Complex::I(0.0, 0.1);

Complex::Complex(double r = 0.0, double i = 0.0)
{
    Real = r;
    Imag = i;
}

// Arithmetic operators
Complex Complex::operator+(const Complex& OtherNumber) const
{
    return Complex(Real + OtherNumber.Real, Imag + OtherNumber.Imag);
}

Complex Complex::operator-(const Complex& OtherNumber) const
{
    return Complex(Real - OtherNumber.Real, Imag - OtherNumber.Imag);
}

Complex Complex::operator*(const Complex& OtherNumber) const
{
    return Complex(Real * OtherNumber.Real - Imag * OtherNumber.Imag,
        Real * OtherNumber.Imag + Imag * OtherNumber.Real);
}

Complex Complex::operator/(const Complex& OtherNumber) const
{
    double RealResult = 0.0;
    double ImagResult = 0.0;

    if ((OtherNumber.Real * OtherNumber.Real + OtherNumber.Imag * OtherNumber.Imag) != 0)
    {
        RealResult = (Real * OtherNumber.Real + Imag * OtherNumber.Imag) / (OtherNumber.Real * OtherNumber.Real + OtherNumber.Imag * OtherNumber.Imag);
        ImagResult = (OtherNumber.Real * Imag - Real * OtherNumber.Imag) / (OtherNumber.Real * OtherNumber.Real + OtherNumber.Imag * OtherNumber.Imag);
    }

    return Complex(RealResult, ImagResult);
}

// Comparison Operators
bool Complex::operator==(const Complex& OtherNumber) const
{
    if (Real == OtherNumber.Real && Imag == OtherNumber.Imag)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Complex::operator!=(const Complex& OtherNumber) const
{
    if (Real != OtherNumber.Real || Imag != OtherNumber.Imag)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Complex::operator>(const Complex& OtherNumber) const
{
    return this->MagnitudeSqr() > OtherNumber.MagnitudeSqr();
}

bool Complex::operator<(const Complex& OtherNumber) const
{
    return this->MagnitudeSqr() < OtherNumber.MagnitudeSqr();
}

bool Complex::operator>=(const Complex& OtherNumber) const
{
    return this->MagnitudeSqr() >= OtherNumber.MagnitudeSqr();
}

bool Complex::operator<=(const Complex& OtherNumber) const
{
    return this->MagnitudeSqr() <= OtherNumber.MagnitudeSqr();
}

// Assignment Operators
Complex& Complex::operator=(const Complex& OtherNumber)
{
    if (this != &OtherNumber)
    {
        Real = OtherNumber.Real;
        Imag = OtherNumber.Imag;
    }
    return *this;
}

Complex& Complex::operator+=(const Complex& OtherNumber)
{
    Real += OtherNumber.Real;
    Imag += OtherNumber.Imag;
    return *this;
}

Complex& Complex::operator-=(const Complex& OtherNumber)
{
    Real -= OtherNumber.Real;
    Imag -= OtherNumber.Imag;
    return *this;
}

Complex& Complex::operator*=(const Complex& OtherNumber)
{
    double NewReal = Real * OtherNumber.Real - Imag * OtherNumber.Imag;
    double NewImag = Real * OtherNumber.Imag + Imag * OtherNumber.Real;
    Real = NewReal;
    Imag = NewImag;
    return *this;
}

Complex& Complex::operator/=(const Complex& OtherNumber)
{
    if (OtherNumber != 0)
    {
        *this = *this / OtherNumber;
    }

    return *this;
}

// Unary operators
Complex Complex::operator+()
{
    return *this;
}

Complex Complex::operator-()
{
    return Complex(-Real, -Imag);
}

// Increment/decrement operators
Complex& Complex::operator++() // Prefix
{
    ++Real;
    return *this;
}

Complex Complex::operator++(int) // Postfix
{
    Complex temp = *this;
    ++Real;
    return temp;
}

Complex& Complex::operator--() // Prefix
{
    --Real;
    return *this;
}

Complex Complex::operator--(int) // Postfix
{
    Complex temp = *this;
    --Real;
    return temp;
}

// Access Methods
double Complex::GetReal() const
{
    return Real;
}

double Complex::GetImag() const
{
    return Imag;
}

void Complex::SetReal(double r)
{
    Real = r;
}

void Complex::SetImage(double i)
{
    Imag = i;
}

// Basic mathematical operations
double Complex::Magnitude() const
{
    return std::sqrt(Real * Real + Imag * Imag);
}

double Complex::Phase() const
{
    return std::atan2(Imag, Real);
}

Complex Complex::Conjugate() const
{
    return Complex(Real, -Imag);
}

double Complex::MagnitudeSqr() const
{
    return Real * Real + Imag * Imag;
}

// Obtaining components in polar form
double Complex::GetMagnitude() const
{
    return Magnitude();
}

double Complex::GetPhase() const
{
    return Phase();
}

// Phase in degrees
double Complex::GetPhaseDegrees() const
{
    return Phase() * 180.0 / PI;
}

// Setting a value using polar coordinates
void Complex::SetPolar(double Magnitude, double Phase)
{
    Real = Magnitude * std::cos(Phase);
    Imag = Magnitude * std::sin(Phase);
}

void Complex::NormalizedPhase()
{
    double Mag = Magnitude();
    double phase = Phase();

    // Phase normalization to the range [-PI/2, PI/2]
    while (phase > PI)
    {
        phase -= 2 * PI;
    }
    while (phase <= PI)
    {
        phase += 2 * PI;
    }

    SetPolar(Mag, phase);
}

// Rotation of a complex number by an angle
Complex Complex::Rotate(double Angle) const
{
    return *this * FromPolar(1.0, Angle);
}

// Trigonometric functions
Complex Complex::Sin() const
{
    return Complex(std::sin(Real) * std::cosh(Imag),
        std::cos(Real) * std::sinh(Imag));
}

Complex Complex::Cos() const
{
    return Complex(std::cos(Real) * std::cosh(Imag), 
        -std::sin(Real) * std::sinh(Imag));
}

Complex Complex::Tan() const
{
    Complex cosValue = Cos();
    if (cosValue != Zero)
    {
        return Sin() / cosValue;
    }
    else
    {
        return Complex(0.0, 0.0);
    }
}

// Hyperbolic functions
Complex Complex::Sinh() const
{
    return Complex(std::sinh(Real) * std::cos(Imag),
        std::cosh(Real) * std::sin(Imag));
}

Complex Complex::Cosh() const
{
    return Complex(std::cosh(Real) * std::cos(Imag),
        std::sinh(Real) * std::sin(Imag));
}

Complex Complex::Tanh() const
{
    Complex coshValue = Cosh();
    if (coshValue != Zero)
    {
        return Sinh() / coshValue;
    }
    else
    {
        return Complex(0.0, 0.0);
    }
}

// Secant
Complex Complex::Sec() const
{
    Complex cosValue = Cos();
    if (cosValue != Zero)
    {
        return One / cosValue;
    }
    else
    {
        return Complex(0.0, 0.0);
    }
}

// Cosecant
Complex Complex::Csc() const
{
    Complex sinValue = Sin();
    if (sinValue != Zero)
    {
        return One / sinValue;
    }
    else
    {
        return Complex(0.0, 0.0);
    }
}

// Cotangent
Complex Complex::Cot() const
{
    Complex tanValue = Tan();
    if (tanValue != Zero)
    {
        return One / tanValue;
    }
    else
    {
        return Complex(0.0, 0.0);
    }
}

// Exponential and logarithmic functions
Complex Complex::Exp() const
{
    double ExpReal = std::exp(Real);
    return Complex(ExpReal * std::cos(Imag),
        ExpReal * std::sin(Imag));
}

Complex Complex::Log() const
{
    return Complex(std::log(this->Magnitude()), this->Phase());
}

Complex Complex::Log10() const
{
    return this->Log() / std::log(10.0);
}

// Power functions
Complex Complex::Pow(double Exponent) const
{
    double Mag = std::pow(this->Magnitude(), Exponent);
    double Phase = this->Phase() * Exponent;
    return Complex(Mag * std::cos(Phase), Mag * std::sin(Phase));
}

Complex Complex::Pow(const Complex& Exponent) const
{
    return (Exponent * this->Log()).Exp();
}

Complex Complex::Sqrt() const
{
    return this->Pow(0.5);
}

// Inverse trigonometric functions
Complex Complex::Asin() const
{
    Complex i(0, 1);
    return -i * ((*this * i + (One - this->Pow(2)).Sqrt()).Log());
}

Complex Complex::Acos() const
{
    Complex i(0, 1);
    return (i * ((*this * i + (One - this->Pow(2)).Sqrt()).Log()) - (PI / 2));
}

Complex Complex::Atan() const
{
    Complex i(0, 1);
    if (i - *this != 0)
    {
        return (i / 2) * ((i + *this) / (i - *this)).Log();
    }
    else
    {
        return Complex(0.0, 0.0);
    }
}

// Area hyperbolic sine
Complex Complex::Asinh() const
{
    return ((*this) + ((*this) * (*this) + One).Sqrt()).Log();
}

// Area hyperbolic cosine
Complex Complex::Acosh() const
{
    return ((*this) + ((*this) * (*this) - One).Sqrt()).Log();
}

// Area hyperbolic tangent
Complex Complex::Atanh() const
{
    if (*this == One || *this == -1 * One)
    {
        return Complex(0.0, 0.0);
    }
    else
    {
        return (0.5) * ((One + (*this)) / (One - (*this))).Log();
    }
}

// =====Helper Methods=====
// Checking for a real number
bool Complex::IsReal() const
{
    return Imag == 0.0;
}

// Testing for a purely imaginary number
bool Complex::IsImaginary() const
{
    return Real == 0.0 && Imag != 0.0;
}

// Checking for zero
bool Complex::IsZero() const
{
    return Real == 0.0 && Imag == 0.0;
}

// Limb check
bool Complex::IsFinite() const
{
    return std::isfinite(Real) && std::isfinite(Imag);
}

// String representation
std::string Complex::ToString() const
{
    if (IsZero())
    {
        return "0";
    }

    std::string Result;
    if (Real != 0.0)
    {
        Result += std::to_string(Real);
    }

    if (Imag != 0.0)
    {
        if (Imag > 0.0 && Real != 0.0)
        {
            Result += "+";
        }
        if (Imag == 1.0)
        {
            Result += "i";
        }
        else if (Imag == -1.0)
        {
            Result += "-i";
        }
        else
        {
            Result += std::to_string(Imag) + "i";
        }
    }

    return Result;
}

// Static factory methods
Complex Complex::FromPolar(double Magnitude, double Phase)
{
    return Complex(Magnitude * std::cos(Phase), Magnitude * std::sin(Phase));
}

Complex Complex::UnitReal()
{
    return One;
}

Complex Complex::UnitImaginary()
{
    return I;
}

// Root of the nth degree (all values)
std::vector<Complex> Complex::Roots(int N) const
{
    if (N <= 0)
    {
        return {};
    }

    std::vector<Complex> Roots;
    double Mag = std::pow(Magnitude(), 1.0 / N);
    double phase = Phase();

    double rootPhase;
    for (int k = 0; k < N; k++)
    {
        rootPhase = (phase + 2 * PI * k) / N;
        Roots.push_back(FromPolar(Mag, rootPhase));
    }

    return Roots;
}

// Generating roots of unity
std::vector<Complex> Complex::RootsOfUnity(int N) const
{
    return Complex::One.Roots(N);
}

// Distance on a unit circle
double Complex::DistanceTo(const Complex& OtherNumber) const
{
    return (*this - OtherNumber).Magnitude();
}

bool Complex::IsOnUnitCircle(double Tolerance) const
{
    return std::abs(MagnitudeSqr() - 1.0) < Tolerance;
}

// In polar form
std::string Complex::ToPolarString() const
{
    double mag = Magnitude();
    double phase = Phase();
    return std::to_string(mag) + " angle " + std::to_string(phase);
}

// In exponential form
std::string Complex::ToExponentString() const
{
    double mag = Magnitude();
    double phase = Phase();
    return std::to_string(mag) + "e^(i" + std::to_string(phase) + ")";
}

// External operators for working with doubles
Complex operator+(double ihs, const Complex rhs)
{
    return Complex(ihs) + rhs;
}

Complex operator-(double ihs, const Complex rhs)
{
    return Complex(ihs) - rhs;
}

Complex operator*(double ihs, const Complex rhs)
{
    return Complex(ihs) * rhs;
}

Complex operator/(double ihs, const Complex rhs)
{
    if (rhs != 0)
    {
        return Complex(ihs) / rhs;
    }
    else
    {
        return Complex(0.0, 0.0);
    }
}

// Streaming output
std::ostream& operator<<(std::ostream& Output, const Complex& ComplexNumber)
{
    Output << ComplexNumber.ToString();
    return Output;
}
