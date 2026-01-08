#include "PlaneTransforms.h"
#include <cmath>
#include <algorithm>
#include <sstream>

const double PI = 3.1415926535897932;

// Homothety implementation
Homothety::Homothety(const Complex& ComplexNumber, double K)
{
	Center = ComplexNumber;
	Coefficient = K;
}

Homothety::Homothety(double CenterX, double CenterY, double K)
{
	Center = Complex(CenterX, CenterY);
	Coefficient = K;
}

Complex Homothety::GetCenter() const
{
	return Center;
}

double Homothety::GetCoefficient() const
{
	return Coefficient;
}

void Homothety::SetCenter(const Complex& ComplexNumber)
{
	Center = ComplexNumber;
}

void Homothety::SetCenter(double X, double Y)
{
	Center = Complex(X, Y);
}

void Homothety::SetCoefficient(double K)
{
	Coefficient = K;
}

Complex Homothety::ApplyTo(const Complex& Point) const
{
	return Center + (Point - Center) * Coefficient;
}

Homothety Homothety::ComposeWith(const Homothety& Other) const
{
	if (Center == Other.Center)
	{
		return Homothety(Center, Coefficient * Other.Coefficient);
	}

	double NewCoefficient = Coefficient * Other.Coefficient;
	Complex NewCenter = (Center * (1 - Other.Coefficient) + Other.Center * (Other.Coefficient * (1 - Coefficient)))
		/ (1 - Coefficient * Other.Coefficient);

	return Homothety(NewCenter, NewCoefficient);
}

Homothety Homothety::getInverse() const
{
	if (Coefficient == 0.0)
	{
		return Homothety();
	}
	else
	{
		return Homothety(Center, 1.0 / Coefficient);
	}
}

Homothety Homothety::RaisedTo(int Number) const
{
	return Homothety(Center, std::pow(Coefficient, Number));
}

bool Homothety::IsExpansion() const
{
	return Coefficient > 1.0;
}

bool Homothety::IsContraction() const
{
	return Coefficient > 0.0 && Coefficient < 1.0;
}

bool Homothety::IsReflection() const
{
	return Coefficient < 0.0;
}

bool Homothety::IsIdentity() const
{
	return false;
}

double Homothety::GetScaleFactor() const
{
	return 0.0;
}

Complex Homothety::GetFixedPointers() const
{
	// For a homothety, the only fixed point is the center (only if the coefficient is not equal to one)
	if (std::abs(Coefficient - 1.0) > 1e-10)
	{
		return Center;
	}
	else // If the coefficient is equal to one, then all points are fixed (identity transformation)
	{
		return Complex();
	}
}

std::vector<Complex> Homothety::ApplyToPolygon(const std::vector<Complex>& Polygon) const
{
	std::vector<Complex> Result;
	Result.reserve(Polygon.size());
	for (const auto& Point : Polygon)
	{
		Result.push_back(ApplyTo(Point));
	}

	return Result;
}

std::pair<Complex, double> Homothety::ApplyToCircle(const Complex& Center, double Radius) const
{
	Complex NewCenter = ApplyTo(Center);
	double NewRadius = std::abs(Coefficient) * Radius;
	return {NewCenter, NewRadius};
}

Homothety Homothety::FromScaleFactor(double Scale)
{
	return Homothety(Complex(0, 0), Scale);
}

Homothety Homothety::FromFixedPointAndImage(const Complex& FixedPoint, const Complex& ImagePoint)
{
	if (FixedPoint == ImagePoint)
	{
		return Homothety(FixedPoint, 1.0); // Identity
	}

	// For a homothety centered at a fixed point, the old function won't work.
    // (ImagePoint = FixedPoint + K * (FixedPoint))
    // Instead, let's create a homothety centered at the origin.

	double K = ImagePoint.Magnitude() / FixedPoint.Magnitude();
	return Homothety(Complex(0, 0), K);
}

std::unique_ptr<PlaneTransformation> Homothety::GetInverse() const
{
	return std::make_unique<Homothety>(getInverse());
}

bool Homothety::operator==(const Homothety& OtherNumber) const
{
	return Center == OtherNumber.Center &&
		std::abs(Coefficient - OtherNumber.Coefficient) < 1e-10;
}

bool Homothety::operator!=(const Homothety& OtherNumber) const
{
	return !(*this == OtherNumber);
}

std::string Homothety::ToString() const
{
	std::stringstream Stream;
	Stream << "Homothety(center=" << Center << ", K= " << Coefficient << ")";
	return Stream.str();
}

Homothety Homothety::FromTwoPairs(const Complex& A, const Complex& A1, const Complex& B, const Complex& B1)
{
	Complex AB = B - A;
	Complex A1B1 = B1 - A1;

	if (AB.Magnitude() < 1e-10)
	{
		return Homothety();
	}
	else
	{
		double K = A1B1.Magnitude() / AB.Magnitude();
		double CrossProduct = AB.GetReal() * A1B1.GetImag() - AB.GetImag() * A1B1.GetReal();

		if (CrossProduct < 0)
		{
			K = -K;
		}

		Complex center = (A1 - A * K) / (1 - K);
		return Homothety(center, K);
	}
}

std::ostream& operator<<(std::ostream& Output, const Homothety& H)
{
	Output << H.ToString();
	return Output;
}

// AffineTransform implementation (Advanced)
AffineTransform::AffineTransform(double matrix11, double matrix12, double matrix13, double matrix21, double matrix22, double matrix23)
{
	Matrix11 = matrix11;
	Matrix12 = matrix12;
	Matrix13 = matrix13;
	Matrix21 = matrix21;
	Matrix22 = matrix22;
	Matrix23 = matrix23;
}

AffineTransform AffineTransform::Identity()
{
	return AffineTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0);
}

AffineTransform AffineTransform::Translation(double tx, double ty)
{
	return AffineTransform(1.0, 0.0, tx, 0.0, 1.0, ty);
}

AffineTransform AffineTransform::Translation(const Complex& Vector)
{
	return Translation(Vector.GetReal(), Vector.GetImag());
}

AffineTransform AffineTransform::Scaling(double sx, double sy)
{
	return AffineTransform(sx, 0.0, 0.0, 0.0, sy, 0.0);
}

AffineTransform AffineTransform::Scaling(double Scale)
{
	return Scaling(Scale, Scale);
}

AffineTransform AffineTransform::Scaling(const Complex& Center, double sx, double sy)
{
	return Translation(Center) * Scaling(sx, sy) * Translation(Center * -1);
}

AffineTransform AffineTransform::Rotation(double AngleRadius)
{
	double cosA = std::cos(AngleRadius);
	double sinA = std::sin(AngleRadius);
	return AffineTransform(cosA, -sinA, 0.0, sinA, cosA, 0.0);
}

AffineTransform AffineTransform::Rotation(double AngleRadius, const Complex& Center)
{
	return Translation(Center) * Rotation(AngleRadius) * Translation(Center * -1);
}

AffineTransform AffineTransform::RotationDegrees(double AngleDegrees)
{
	return Rotation(AngleDegrees * PI / 180.0);
}

AffineTransform AffineTransform::RotationDegrees(double AngleDegrees, const Complex& Center)
{
	return Rotation(AngleDegrees * PI / 180.0, Center);
}

AffineTransform AffineTransform::Shear(double shx, double shy)
{
	return AffineTransform(1.0, shx, 0.0, shy, 1.0, 0.0);
}

AffineTransform AffineTransform::ReflectionOverLine(const Complex& FirstPoint, const Complex& SecondPoint)
{
	Complex Direction = SecondPoint - FirstPoint;
	double Angle = std::atan2(Direction.GetImag(), Direction.GetReal());

	// We rotate the coordinate system, reflect it, and rotate it back
	return Translation(FirstPoint) * Rotation(-Angle) * Scaling(1, -1) * Rotation(Angle) * Translation(FirstPoint * -1);
}

AffineTransform AffineTransform::ReflectionOverXAxis()
{
	return Scaling(1, -1);
}

AffineTransform AffineTransform::ReflectionOverYAxis()
{
	return Scaling(-1, 1);
}

AffineTransform AffineTransform::ProjectionOntoLine(const Complex& FirstPoint, const Complex& SecondPoint)
{
	Complex Direction = SecondPoint - FirstPoint;
	double Angle = std::atan2(Direction.GetImag(), Direction.GetReal());

	// Projection: rotate, reset the ordinate axis, rotate back
	return Translation(FirstPoint) * Rotation(-Angle) * Scaling(1, 0) * Rotation(Angle) * Translation(FirstPoint * -1);
}

AffineTransform AffineTransform::FromTriangleMapping(const Complex& srcA, const Complex& srcB, const Complex& srcC, const Complex& dstA, const Complex& dstB, const Complex& dstC)
{
	// Solving a system of equations for an affine transformation
	Complex srcAB = srcB - srcA;
	Complex srcAC = srcC - srcA;
	Complex dstAB = srcB - srcA;
	Complex dstAC = dstC - dstA;

	double det = srcAB.GetReal() * srcAC.GetImag() - srcAB.GetImag() * srcAC.GetReal();
	if (std::abs(det) < 1e-10)
	{
		return AffineTransform();
	}
	else
	{
		double matrix11 = (dstAB.GetReal() * srcAC.GetImag() - dstAC.GetReal() * srcAB.GetImag()) / det;
		double matrix12 = (dstAC.GetReal() * srcAB.GetReal() - dstAB.GetReal() * srcAC.GetReal()) / det;
		double matrix21 = (dstAB.GetImag() * srcAC.GetImag() - dstAC.GetImag() * srcAB.GetImag()) / det;
		double matrix22 = (dstAC.GetImag() * srcAB.GetReal() - dstAB.GetImag() * srcAC.GetReal()) / det;

		Complex translation = dstA - Complex(matrix11 * srcA.GetReal() + matrix12 * srcA.GetImag(),
			matrix21 * srcA.GetReal() + matrix22 * srcA.GetImag());

		return AffineTransform(matrix11, matrix12, translation.GetReal(),
			matrix21, matrix22, translation.GetImag());
	}
}

AffineTransform AffineTransform::FromBasisVectors(const Complex& e1, const Complex& e2, const Complex& origin)
{
	return AffineTransform(e1.GetReal(), e2.GetReal(), origin.GetReal(),
		e1.GetImag(), e2.GetImag(), origin.GetImag());
}

Complex AffineTransform::ApplyTo(const Complex& point) const
{
	double x = point.GetReal();
	double y = point.GetImag();
	double new_x = Matrix11 * x + Matrix12 * y + Matrix13;
	double new_y = Matrix21 * x + Matrix22 * y + Matrix23;
	return Complex(new_x, new_y);
}

AffineTransform AffineTransform::ComposeWith(const AffineTransform& other) const
{
	return AffineTransform(
		Matrix11 * other.Matrix11 + Matrix12 * other.Matrix21,
		Matrix11 * other.Matrix12 + Matrix12 * other.Matrix22,
		Matrix11 * other.Matrix13 + Matrix12 * other.Matrix23 + Matrix13,
		Matrix21 * other.Matrix11 + Matrix22 * other.Matrix21,
		Matrix21 * other.Matrix12 + Matrix22 * other.Matrix22,
		Matrix21 * other.Matrix13 + Matrix22 * other.Matrix23 + Matrix23
	);
}

AffineTransform AffineTransform::Inverse() const
{
	double det = Matrix11 * Matrix22 - Matrix12 * Matrix21;
	if (std::abs(det) < 1e-10)
		throw std::runtime_error("Affine transform is not invertible");

	double inv_det = 1.0 / det;
	return AffineTransform(
		Matrix22 * inv_det,
		-Matrix12 * inv_det,
		(Matrix12 * Matrix23 - Matrix22 * Matrix13) * inv_det,
		-Matrix21 * inv_det,
		Matrix11 * inv_det,
		(Matrix21 * Matrix13 - Matrix11 * Matrix23) * inv_det
	);
}

bool AffineTransform::IsIdentity() const
{
	return std::abs(Matrix11 - 1.0) < 1e-10 && std::abs(Matrix12) < 1e-10 &&
		std::abs(Matrix13) < 1e-10 && std::abs(Matrix21) < 1e-10 &&
		std::abs(Matrix22 - 1.0) < 1e-10 && std::abs(Matrix23) < 1e-10;
}

bool AffineTransform::IsIsometry() const
{
	double a = Matrix11 * Matrix11 + Matrix21 * Matrix21;
	double b = Matrix11 * Matrix12 + Matrix21 * Matrix22;
	double c = Matrix12 * Matrix12 + Matrix22 * Matrix22;

	return std::abs(a - 1.0) < 1e-10 &&
		std::abs(b) < 1e-10 &&
		std::abs(c - 1.0) < 1e-10;
}

bool AffineTransform::IsSimilarity() const
{
	// We check that the linear part is similar: A^T * A = kI
	double a = Matrix11 * Matrix11 + Matrix21 * Matrix21;
	double b = Matrix11 * Matrix12 + Matrix21 * Matrix22;
	double c = Matrix12 * Matrix12 + Matrix22 * Matrix22;

	return std::abs(a - c) < 1e-10 && std::abs(b) < 1e-10;
}

bool AffineTransform::IsEquiareal() const
{
	return std::abs(Determinant() - 1.0) < 1e-10;
}

bool AffineTransform::IsDirect() const
{
	return Determinant() > 0;
}

bool AffineTransform::IsInvolutory() const
{
	AffineTransform square = this->ComposeWith(*this);
	return square.IsIdentity();
}

Complex AffineTransform::GetTranslation() const
{
	return Complex(Matrix13, Matrix23);
}

double AffineTransform::GetRotationAngle() const
{
	return std::atan2(Matrix21, Matrix11);
}

Complex AffineTransform::GetScaleFactors() const
{
	double scale_x = std::sqrt(Matrix11 * Matrix11 + Matrix21 * Matrix21);
	double scale_y = std::sqrt(Matrix12 * Matrix12 + Matrix22 * Matrix22);
	return Complex(scale_x, scale_y);
}

Complex AffineTransform::GetShearFactors() const
{
	double shear_x = Matrix12 / Matrix22;  // If Matrix22 != 0
	double shear_y = Matrix21 / Matrix11;  // If Matrix11 != 0
	return Complex(shear_x, shear_y);
}

double AffineTransform::GetAreaScale() const
{
	return std::abs(Determinant());
}

std::vector<Complex> AffineTransform::GetFixedPoints() const
{
	// We solve the equation: T(p) = p
	// => (A-I)p = -b
	double a = Matrix11 - 1.0, b = Matrix12, c = Matrix21, d = Matrix22 - 1.0;
	Complex translation(-Matrix13, -Matrix23); double det = a * d - b * c;
	if (std::abs(det) > 1e-10) 
	{
		// The only fixed point
		double x = (d * translation.GetReal() - b * translation.GetImag()) / det;
		double y = (-c * translation.GetReal() + a * translation.GetImag()) / det;
		return 
		{ 
			Complex(x, y) 
		};
	}
	else 
	{
		// Line of fixed points or no fixed points
		return {};
	}
}

bool AffineTransform::HasFixedPoint(const Complex& point) const
{
	Complex transformed = ApplyTo(point);
	return (point - transformed).Magnitude() < 1e-10;
}

std::vector<Complex> AffineTransform::GetEigenvalues() const
{
	// Characteristic equation: λ^2 - tr(A)λ + det(A) = 0
	double trace = Matrix11 + Matrix22;
	double det = Determinant();

	double discriminant = trace * trace - 4 * det;
	if (discriminant >= 0) 
	{
		double lambda1 = (trace + std::sqrt(discriminant)) / 2;
		double lambda2 = (trace - std::sqrt(discriminant)) / 2;
		return 
		{ 
			Complex(lambda1, 0), Complex(lambda2, 0) 
		};
	}
	else 
	{
		double real = trace / 2;
		double imag = std::sqrt(-discriminant) / 2;
		return 
		{ 
			Complex(real, imag), Complex(real, -imag) 
		};
	}
}

std::vector<Complex> AffineTransform::GetEigenvectors() const
{
	// Simplified version - for real eigenvalues
	auto eigenvalues = GetEigenvalues();
	std::vector<Complex> eigenvectors;

	for (const auto& lambda : eigenvalues) 
	{
		if (std::abs(lambda.GetImag()) < 1e-10) 
		{
			// Real eigenvalue
			double a = Matrix11 - lambda.GetReal();
			double b = Matrix12;
			double c = Matrix21;
			double d = Matrix22 - lambda.GetReal();

			if (std::abs(b) > 1e-10 || std::abs(a) > 1e-10) 
			{
				eigenvectors.push_back(Complex(-b, a));
			}
			else if (std::abs(d) > 1e-10 || std::abs(c) > 1e-10) 
			{
				eigenvectors.push_back(Complex(-d, c));
			}
		}
	}

	return eigenvectors;
}

std::vector<Complex> AffineTransform::ApplyToPolygon(const std::vector<Complex>& polygon) const
{
	std::vector<Complex> result;
	result.reserve(polygon.size());
	for (const auto& point : polygon) 
	{
		result.push_back(ApplyTo(point));
	}
	return result;
}

std::pair<Complex, double> AffineTransform::ApplyToCircle(const Complex& center, double radius) const
{
	Complex newCenter = ApplyTo(center);

	// For an affine transformation, a circle becomes an ellipse
	// Return the circumscribed circle around the ellipse
	double maxScale = std::max(GetScaleFactors().GetReal(), GetScaleFactors().GetImag());
	double newRadius = radius * maxScale;

	return { newCenter, newRadius };
}

Complex AffineTransform::ApplyToVector(const Complex& vector) const
{
	// We use only the linear part (without broadcasting)
	double x = vector.GetReal();
	double y = vector.GetImag();
	return Complex(Matrix11 * x + Matrix12 * y, Matrix21 * x + Matrix22 * y);
}

void AffineTransform::Decompose(Complex& translation, Complex& scale, double& rotation_angle) const
{
	translation = GetTranslation();

	// Using QR decomposition to extract rotation and scale
	double scale_x = std::sqrt(Matrix11 * Matrix11 + Matrix21 * Matrix21);
	double scale_y = (Matrix11 * Matrix22 - Matrix12 * Matrix21) / scale_x; // det / scale_x

	scale = Complex(scale_x, scale_y);
	rotation_angle = std::atan2(Matrix21, Matrix11);
}

std::tuple<AffineTransform, AffineTransform, AffineTransform> AffineTransform::DecomposeLDU() const
{
	// LDU decomposition: A = L * D * U
	// Where L is the lower triangular, D is the diagonal, U is the upper triangular

	if (std::abs(Matrix11) < 1e-10) 
	{
		throw std::runtime_error("LDU decomposition requires Matrix11 != 0");
	}

	// L (lower triangular)
	double l21 = Matrix21 / Matrix11;
	AffineTransform L(1, 0, 0, l21, 1, 0);

	// U (upper triangular)
	AffineTransform U(Matrix11, Matrix12, Matrix13, 0, Matrix22 - l21 * Matrix12, Matrix23 - l21 * Matrix13);

	// D (diagonal) - here unit, since we included the scale in U
	AffineTransform D = Identity();

	return std::make_tuple(L, D, U);
}

AffineTransform AffineTransform::Transpose() const
{
	return AffineTransform(Matrix11, Matrix21, Matrix13,
		Matrix12, Matrix22, Matrix23);
}

double AffineTransform::Trace() const
{
	return Matrix11 + Matrix22;
}

double AffineTransform::Determinant() const
{
	return Matrix11 * Matrix22 - Matrix12 * Matrix21;
}

AffineTransform AffineTransform::Adjugate() const
{
	return AffineTransform(Matrix22, -Matrix12, Matrix12 * Matrix23 - Matrix22 * Matrix13,
		-Matrix21, Matrix11, Matrix21 * Matrix13 - Matrix11 * Matrix23);
}

AffineTransform AffineTransform::Power(int n) const
{
	if (n == 0) return Identity();
	if (n == 1) return *this;
	if (n < 0) return Inverse().Power(-n);

	AffineTransform result = Identity();
	AffineTransform base = *this;

	while (n > 0) 
	{
		if (n % 2 == 1) 
		{
			result = result.ComposeWith(base);
		}
		base = base.ComposeWith(base);
		n /= 2;
	}

	return result;
}

AffineTransform AffineTransform::Lerp(const AffineTransform& a, const AffineTransform& b, double t)
{
	return AffineTransform(
		a.Matrix11 + t * (b.Matrix11 - a.Matrix11),
		a.Matrix12 + t * (b.Matrix12 - a.Matrix12),
		a.Matrix13 + t * (b.Matrix13 - a.Matrix13),
		a.Matrix21 + t * (b.Matrix21 - a.Matrix21),
		a.Matrix22 + t * (b.Matrix22 - a.Matrix22),
		a.Matrix23 + t * (b.Matrix23 - a.Matrix23)
	);
}

std::unique_ptr<PlaneTransformation> AffineTransform::GetInverse() const
{
	return std::make_unique<AffineTransform>(Inverse());
}

std::unique_ptr<PlaneTransformation> AffineTransform::ComposeWith(const PlaneTransformation& other) const
{
	if (const AffineTransform* otherAffine = dynamic_cast<const AffineTransform*>(&other)) 
	{
		return std::make_unique<AffineTransform>(ComposeWith(*otherAffine));
	}
	auto composite = std::make_unique<CompositeTransformation>();
	composite->AddTransformation(std::make_unique<AffineTransform>(*this));
	composite->AddTransformation(other.GetInverse()->GetInverse());

	return composite;
}

void AffineTransform::GetMatrix(double& a11, double& a12, double& a13,
	double& a21, double& a22, double& a23) const
{
	a11 = Matrix11; a12 = Matrix12; a13 = Matrix13;
	a21 = Matrix21; a22 = Matrix22; a23 = Matrix23;
}

void AffineTransform::GetLinearPart(double& a11, double& a12, double& a21, double& a22) const
{
	a11 = Matrix11; a12 = Matrix12;
	a21 = Matrix21; a22 = Matrix22;
}

void AffineTransform::GetTranslation(double& tx, double& ty) const
{
	tx = Matrix13; ty = Matrix23;
}

std::string AffineTransform::ToString() const
{
	std::stringstream ss;
	ss << "AffineTransform(["
		<< Matrix11 << ", " << Matrix12 << ", " << Matrix13 << "], ["
		<< Matrix21 << ", " << Matrix22 << ", " << Matrix23 << "])";
	return ss.str();
}

std::string AffineTransform::ToMatrixString() const
{
	std::stringstream ss;
	ss << "[[" << Matrix11 << ", " << Matrix12 << ", " << Matrix13 << "],\n"
		<< " [" << Matrix21 << ", " << Matrix22 << ", " << Matrix23 << "]]";
	return ss.str();
}

bool AffineTransform::operator==(const AffineTransform& other) const
{
	return std::abs(Matrix11 - other.Matrix11) < 1e-10 &&
		std::abs(Matrix12 - other.Matrix12) < 1e-10 &&
		std::abs(Matrix13 - other.Matrix13) < 1e-10 &&
		std::abs(Matrix21 - other.Matrix21) < 1e-10 &&
		std::abs(Matrix22 - other.Matrix22) < 1e-10 &&
		std::abs(Matrix23 - other.Matrix23) < 1e-10;
}

bool AffineTransform::operator!=(const AffineTransform& other) const
{
	return !(*this == other);
}

std::ostream& operator<<(std::ostream& Output, const AffineTransform& Transform)
{
	Output << Transform.ToString();
	return Output;
}
