#include "PlaneTransforms.h"
#include <cmath>
#include <algorithm>
#include <sstream>

const double PI = 3.1415926535897932;

// Homothety implementation
Homothety::Homothety(const Complex& complexNumber, double k) {
	center = complexNumber;
	coefficient = k;
}

Homothety::Homothety(double centerX, double centerY, double k) {
	center = Complex(centerX, centerY);
	coefficient = k;
}

Complex Homothety::getCenter() const {
	return center;
}

double Homothety::getCoefficient() const {
	return coefficient;
}

void Homothety::setCenter(const Complex& complexNumber) {
	center = complexNumber;
}

void Homothety::setCenter(double x, double y) {
	center = Complex(x, y);
}

void Homothety::setCoefficient(double k) {
	coefficient = k;
}

Complex Homothety::applyTo(const Complex& point) const {
	return center + (point - center) * coefficient;
}

Homothety Homothety::composeWith(const Homothety& other) const {
	if (center == other.center) {
		return Homothety(center, coefficient * other.coefficient);
	}

	double newCoefficient = coefficient * other.coefficient;
	Complex newCenter = (center * (1 - other.coefficient) + other.center * (other.coefficient * (1 - coefficient)))
		/ (1 - coefficient * other.coefficient);

	return Homothety(newCenter, newCoefficient);
}

Homothety Homothety::getInverse() {
	if (coefficient == 0.0) {
		return Homothety();
	}
	else {
		return Homothety(center, 1.0 / coefficient);
	}
}

Homothety Homothety::raisedTo(int number) const {
	return Homothety(center, std::pow(coefficient, number));
}

bool Homothety::isExpansion() const {
	return coefficient > 1.0;
}

bool Homothety::isContraction() const {
	return coefficient > 0.0 && coefficient < 1.0;
}

bool Homothety::isReflection() const {
	return coefficient < 0.0;
}

bool Homothety::isIdentity() const {
	return false;
}

double Homothety::getScaleFactor() const {
	return 0.0;
}

Complex Homothety::getFixedPointers() const {
	// For a homothety, the only fixed point is the center (only if the coefficient is not equal to one)
	if (std::abs(coefficient - 1.0) > 1e-10) {
		return center;
	}
	else {
		// If the coefficient is equal to one, then all points are fixed (identity transformation)
		return Complex();
	}
}

std::vector<Complex> Homothety::applyToPolygon(const std::vector<Complex>& polygon) const {
	std::vector<Complex> result;
	result.reserve(polygon.size());
	for (const auto& point : polygon) {
		result.push_back(applyTo(point));
	}

	return result;
}

std::pair<Complex, double> Homothety::applyToCircle(const Complex& center, double radius) const {
	Complex newCenter = applyTo(center);
	double newRadius = std::abs(coefficient) * radius;

	return { newCenter, newRadius };
}

Homothety Homothety::fromScaleFactor(double scale) {
	return Homothety(Complex(0, 0), scale);
}

Homothety Homothety::fromFixedPointAndImage(const Complex& fixedPoint, const Complex& imagePoint) {
	if (fixedPoint == imagePoint) {
		return Homothety(fixedPoint, 1.0); // Identity
	}

	// For a homothety centered at a fixed point, the old function won't work.
    // (ImagePoint = FixedPoint + K * (FixedPoint))
    // Instead, let's create a homothety centered at the origin.

	double k = imagePoint.magnitude() / fixedPoint.magnitude();
	return Homothety(Complex(0, 0), k);
}

std::unique_ptr<PlaneTransformation> Homothety::getInverse() const {
	return std::make_unique<Homothety>(getInverse());
}

bool Homothety::operator==(const Homothety& otherNumber) const {
	return center == otherNumber.center &&
		std::abs(coefficient - otherNumber.coefficient) < 1e-10;
}

bool Homothety::operator!=(const Homothety& otherNumber) const {
	return !(*this == otherNumber);
}

std::string Homothety::toString() const {
	std::stringstream stream;
	stream << "Homothety(center=" << center << ", K= " << coefficient << ")";

	return stream.str();
}

Homothety Homothety::fromTwoPairs(const Complex& a, const Complex& a1, const Complex& b, const Complex& b1) {
	Complex ab = b - a;
	Complex a1b1 = b1 - a1;

	if (ab.magnitude() < 1e-10)	{
		return Homothety();
	}
	else {
		double k = a1b1.magnitude() / ab.magnitude();
		double crossProduct = ab.getReal() * a1b1.getImag() - ab.getImag() * a1b1.getReal();

		if (crossProduct < 0) {
			k = -k;
		}

		Complex center = (a1 - a * k) / (1 - k);
		return Homothety(center, k);
	}
}

std::ostream& operator<<(std::ostream& output, const Homothety& h) {
	output << h.toString();
	return output;
}

// AffineTransform implementation (Advanced)
AffineTransform::AffineTransform(double matrix11Value, double matrix12Value, double matrix13Value, double matrix21Value, double matrix22Value, double matrix23Value) {
	matrix11 = matrix11Value;
	matrix12 = matrix12Value;
	matrix13 = matrix13Value;
	matrix21 = matrix21Value;
	matrix22 = matrix22Value;
	matrix23 = matrix23Value;
}

AffineTransform AffineTransform::identity() {
	return AffineTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0);
}

AffineTransform AffineTransform::translation(double tx, double ty) {
	return AffineTransform(1.0, 0.0, tx, 0.0, 1.0, ty);
}

AffineTransform AffineTransform::translation(const Complex& vector) {
	return translation(vector.getReal(), vector.getImag());
}

AffineTransform AffineTransform::scaling(double sx, double sy) {
	return AffineTransform(sx, 0.0, 0.0, 0.0, sy, 0.0);
}

AffineTransform AffineTransform::scaling(double scale) {
	return scaling(scale, scale);
}

AffineTransform AffineTransform::scaling(const Complex& center, double sx, double sy) {
	return translation(center) * scaling(sx, sy) * translation(center * -1);
}

AffineTransform AffineTransform::rotation(double angleRadius) {
	double cosA = std::cos(angleRadius);
	double sinA = std::sin(angleRadius);

	return AffineTransform(cosA, -sinA, 0.0, sinA, cosA, 0.0);
}

AffineTransform AffineTransform::rotation(double angleRadius, const Complex& center) {
	return translation(center) * rotation(angleRadius) * translation(center * -1);
}

AffineTransform AffineTransform::rotationDegrees(double angleDegrees) {
	return rotation(angleDegrees * PI / 180.0);
}

AffineTransform AffineTransform::rotationDegrees(double angleDegrees, const Complex& center) {
	return rotation(angleDegrees * PI / 180.0, center);
}

AffineTransform AffineTransform::shear(double shx, double shy) {
	return AffineTransform(1.0, shx, 0.0, shy, 1.0, 0.0);
}

AffineTransform AffineTransform::reflectionOverLine(const Complex& firstPoint, const Complex& secondPoint) {
	Complex direction = secondPoint - firstPoint;
	double angle = std::atan2(direction.getImag(), direction.getReal());

	// We rotate the coordinate system, reflect it, and rotate it back
	return translation(firstPoint) * rotation(-angle) * scaling(1, -1) * rotation(angle) * translation(firstPoint * -1);
}

AffineTransform AffineTransform::reflectionOverXAxis() {
	return scaling(1, -1);
}

AffineTransform AffineTransform::reflectionOverYAxis() {
	return scaling(-1, 1);
}

AffineTransform AffineTransform::projectionOntoLine(const Complex& firstPoint, const Complex& secondPoint) {
	Complex direction = secondPoint - firstPoint;
	double angle = std::atan2(direction.getImag(), direction.getReal());

	// Projection: rotate, reset the ordinate axis, rotate back
	return translation(firstPoint) * rotation(-angle) * scaling(1, 0) * rotation(angle) * translation(firstPoint * -1);
}

AffineTransform AffineTransform::fromTriangleMapping(const Complex& srcA, const Complex& srcB, const Complex& srcC, const Complex& dstA, const Complex& dstB, const Complex& dstC) {
	// Solving a system of equations for an affine transformation
	Complex srcAB = srcB - srcA;
	Complex srcAC = srcC - srcA;
	Complex dstAB = srcB - srcA;
	Complex dstAC = dstC - dstA;

	double det = srcAB.getReal() * srcAC.getImag() - srcAB.getImag() * srcAC.getReal();
	if (std::abs(det) < 1e-10) {
		return AffineTransform();
	}
	else {
		double matrix11Value = (dstAB.getReal() * srcAC.getImag() - dstAC.getReal() * srcAB.getImag()) / det;
		double matrix12Value = (dstAC.getReal() * srcAB.getReal() - dstAB.getReal() * srcAC.getReal()) / det;
		double matrix21Value = (dstAB.getImag() * srcAC.getImag() - dstAC.getImag() * srcAB.getImag()) / det;
		double matrix22Value = (dstAC.getImag() * srcAB.getReal() - dstAB.getImag() * srcAC.getReal()) / det;

		Complex translation = dstA - Complex(matrix11Value * srcA.getReal() + matrix12Value * srcA.getImag(),
			matrix21Value * srcA.getReal() + matrix22Value * srcA.getImag());

		return AffineTransform(matrix11Value, matrix12Value, translation.getReal(),
			matrix21Value, matrix22Value, translation.getImag());
	}
}

AffineTransform AffineTransform::fromBasisVectors(const Complex& e1, const Complex& e2, const Complex& origin) {
	return AffineTransform(e1.getReal(), e2.getReal(), origin.getReal(),
		e1.getImag(), e2.getImag(), origin.getImag());
}

Complex AffineTransform::applyTo(const Complex& point) const {
	double x = point.getReal();
	double y = point.getImag();
	double new_x = matrix11 * x + matrix12 * y + matrix13;
	double new_y = matrix21 * x + matrix22 * y + matrix23;

	return Complex(new_x, new_y);
}

AffineTransform AffineTransform::composeWith(const AffineTransform& other) const {
	return AffineTransform(
		matrix11 * other.matrix11 + matrix12 * other.matrix21,
		matrix11 * other.matrix12 + matrix12 * other.matrix22,
		matrix11 * other.matrix13 + matrix12 * other.matrix23 + matrix13,
		matrix21 * other.matrix11 + matrix22 * other.matrix21,
		matrix21 * other.matrix12 + matrix22 * other.matrix22,
		matrix21 * other.matrix13 + matrix22 * other.matrix23 + matrix23
	);
}

AffineTransform AffineTransform::inverse() const {
	double det = matrix11 * matrix22 - matrix12 * matrix21;
	if (std::abs(det) < 1e-10)
		throw std::runtime_error("Affine transform is not invertible");

	double inv_det = 1.0 / det;
	return AffineTransform(
		matrix22 * inv_det,
		-matrix12 * inv_det,
		(matrix12 * matrix23 - matrix22 * matrix13) * inv_det,
		-matrix21 * inv_det,
		matrix11 * inv_det,
		(matrix21 * matrix13 - matrix11 * matrix23) * inv_det
	);
}

bool AffineTransform::isIdentity() const {
	return std::abs(matrix11 - 1.0) < 1e-10 && std::abs(matrix12) < 1e-10 &&
		std::abs(matrix13) < 1e-10 && std::abs(matrix21) < 1e-10 &&
		std::abs(matrix22 - 1.0) < 1e-10 && std::abs(matrix23) < 1e-10;
}

bool AffineTransform::isIsometry() const {
	double a = matrix11 * matrix11 + matrix21 * matrix21;
	double b = matrix11 * matrix12 + matrix21 * matrix22;
	double c = matrix12 * matrix12 + matrix22 * matrix22;

	return std::abs(a - 1.0) < 1e-10 &&
		std::abs(b) < 1e-10 &&
		std::abs(c - 1.0) < 1e-10;
}

bool AffineTransform::isSimilarity() const {
	// We check that the linear part is similar: A^T * A = kI
	double a = matrix11 * matrix11 + matrix21 * matrix21;
	double b = matrix11 * matrix12 + matrix21 * matrix22;
	double c = matrix12 * matrix12 + matrix22 * matrix22;

	return std::abs(a - c) < 1e-10 && std::abs(b) < 1e-10;
}

bool AffineTransform::isEquiareal() const {
	return std::abs(determinant() - 1.0) < 1e-10;
}

bool AffineTransform::isDirect() const {
	return determinant() > 0;
}

bool AffineTransform::isInvolutory() const {
	AffineTransform square = this->composeWith(*this);
	return square.isIdentity();
}

Complex AffineTransform::getTranslation() const {
	return Complex(matrix13, matrix23);
}

double AffineTransform::getRotationAngle() const {
	return std::atan2(matrix21, matrix11);
}

Complex AffineTransform::getScaleFactors() const {
	double scale_x = std::sqrt(matrix11 * matrix11 + matrix21 * matrix21);
	double scale_y = std::sqrt(matrix12 * matrix12 + matrix22 * matrix22);

	return Complex(scale_x, scale_y);
}

Complex AffineTransform::getShearFactors() const {
	double shear_x = matrix12 / matrix22;  // If Matrix22 != 0
	double shear_y = matrix21 / matrix11;  // If Matrix11 != 0

	return Complex(shear_x, shear_y);
}

double AffineTransform::getAreaScale() const {
	return std::abs(determinant());
}

std::vector<Complex> AffineTransform::getFixedPoints() const {
	// We solve the equation: T(p) = p
	// => (A-I)p = -b
	double a = matrix11 - 1.0, b = matrix12, c = matrix21, d = matrix22 - 1.0;
	Complex translation(-matrix13, -matrix23); double det = a * d - b * c;
	if (std::abs(det) > 1e-10) {
		// The only fixed point
		double x = (d * translation.getReal() - b * translation.getImag()) / det;
		double y = (-c * translation.getReal() + a * translation.getImag()) / det;
		return { 
			Complex(x, y) 
		};
	}
	else {
		// Line of fixed points or no fixed points
		return {};
	}
}

bool AffineTransform::hasFixedPoint(const Complex& point) const {
	Complex transformed = applyTo(point);
	return (point - transformed).magnitude() < 1e-10;
}

std::vector<Complex> AffineTransform::getEigenvalues() const {
	// Characteristic equation: λ^2 - tr(A)λ + det(A) = 0
	double trace = matrix11 + matrix22;
	double det = determinant();

	double discriminant = trace * trace - 4 * det;
	if (discriminant >= 0) {
		double lambda1 = (trace + std::sqrt(discriminant)) / 2;
		double lambda2 = (trace - std::sqrt(discriminant)) / 2;
		return { 
			Complex(lambda1, 0), Complex(lambda2, 0) 
		};
	}
	else {
		double real = trace / 2;
		double imag = std::sqrt(-discriminant) / 2;
		return { 
			Complex(real, imag), Complex(real, -imag) 
		};
	}
}

std::vector<Complex> AffineTransform::getEigenvectors() const {
	// Simplified version - for real eigenvalues
	auto eigenvalues = getEigenvalues();
	std::vector<Complex> eigenvectors;

	for (const auto& lambda : eigenvalues) {
		if (std::abs(lambda.getImag()) < 1e-10) {
			// Real eigenvalue
			double a = matrix11 - lambda.getReal();
			double b = matrix12;
			double c = matrix21;
			double d = matrix22 - lambda.getReal();

			if (std::abs(b) > 1e-10 || std::abs(a) > 1e-10) {
				eigenvectors.push_back(Complex(-b, a));
			}
			else if (std::abs(d) > 1e-10 || std::abs(c) > 1e-10) {
				eigenvectors.push_back(Complex(-d, c));
			}
		}
	}

	return eigenvectors;
}

std::vector<Complex> AffineTransform::applyToPolygon(const std::vector<Complex>& polygon) const {
	std::vector<Complex> result;
	result.reserve(polygon.size());
	for (const auto& point : polygon) {
		result.push_back(applyTo(point));
	}

	return result;
}

std::pair<Complex, double> AffineTransform::applyToCircle(const Complex& center, double radius) const {
	Complex newCenter = applyTo(center);

	// For an affine transformation, a circle becomes an ellipse
	// Return the circumscribed circle around the ellipse
	double maxScale = std::max(getScaleFactors().getReal(), getScaleFactors().getImag());
	double newRadius = radius * maxScale;

	return { newCenter, newRadius };
}

Complex AffineTransform::applyToVector(const Complex& vector) const {
	// We use only the linear part (without broadcasting)
	double x = vector.getReal();
	double y = vector.getImag();

	return Complex(matrix11 * x + matrix12 * y, matrix21 * x + matrix22 * y);
}

void AffineTransform::decompose(Complex& translation, Complex& scale, double& rotation_angle) const {
	translation = getTranslation();

	// Using QR decomposition to extract rotation and scale
	double scale_x = std::sqrt(matrix11 * matrix11 + matrix21 * matrix21);
	double scale_y = (matrix11 * matrix22 - matrix12 * matrix21) / scale_x; // det / scale_x

	scale = Complex(scale_x, scale_y);
	rotation_angle = std::atan2(matrix21, matrix11);
}

std::tuple<AffineTransform, AffineTransform, AffineTransform> AffineTransform::decomposeLDU() const {
	// LDU decomposition: A = L * D * U
	// Where L is the lower triangular, D is the diagonal, U is the upper triangular

	if (std::abs(matrix11) < 1e-10)	{
		throw std::runtime_error("LDU decomposition requires Matrix11 != 0");
	}

	// L (lower triangular)
	double l21 = matrix21 / matrix11;
	AffineTransform l(1, 0, 0, l21, 1, 0);

	// U (upper triangular)
	AffineTransform u(matrix11, matrix12, matrix13, 0, matrix22 - l21 * matrix12, matrix23 - l21 * matrix13);

	// D (diagonal) - here unit, since we included the scale in U
	AffineTransform d = identity();

	return std::make_tuple(l, d, u);
}

AffineTransform AffineTransform::transpose() const {
	return AffineTransform(matrix11, matrix21, matrix13,
		matrix12, matrix22, matrix23);
}

double AffineTransform::trace() const {
	return matrix11 + matrix22;
}

double AffineTransform::determinant() const {
	return matrix11 * matrix22 - matrix12 * matrix21;
}

AffineTransform AffineTransform::adjugate() const {
	return AffineTransform(matrix22, -matrix12, matrix12 * matrix23 - matrix22 * matrix13,
		-matrix21, matrix11, matrix21 * matrix13 - matrix11 * matrix23);
}

AffineTransform AffineTransform::power(int n) const {
	if (n == 0) return identity();
	if (n == 1) return *this;
	if (n < 0) return inverse().power(-n);

	AffineTransform result = identity();
	AffineTransform base = *this;

	while (n > 0) {
		if (n % 2 == 1) {
			result = result.composeWith(base);
		}
		base = base.composeWith(base);
		n /= 2;
	}

	return result;
}

AffineTransform AffineTransform::lerp(const AffineTransform& a, const AffineTransform& b, double t) {
	return AffineTransform(
		a.matrix11 + t * (b.matrix11 - a.matrix11),
		a.matrix12 + t * (b.matrix12 - a.matrix12),
		a.matrix13 + t * (b.matrix13 - a.matrix13),
		a.matrix21 + t * (b.matrix21 - a.matrix21),
		a.matrix22 + t * (b.matrix22 - a.matrix22),
		a.matrix23 + t * (b.matrix23 - a.matrix23)
	);
}

std::unique_ptr<PlaneTransformation> AffineTransform::getInverse() const {
	return std::make_unique<AffineTransform>(inverse());
}

std::unique_ptr<PlaneTransformation> AffineTransform::composeWith(const PlaneTransformation& other) const {
	if (const AffineTransform* otherAffine = dynamic_cast<const AffineTransform*>(&other)) {
		return std::make_unique<AffineTransform>(composeWith(*otherAffine));
	}
	auto composite = std::make_unique<CompositeTransformation>();
	composite->addTransformation(std::make_unique<AffineTransform>(*this));
	composite->addTransformation(other.getInverse()->getInverse());

	return composite;
}

void AffineTransform::getMatrix(double& a11, double& a12, double& a13, double& a21, double& a22, double& a23) const {
	a11 = matrix11; a12 = matrix12; a13 = matrix13;
	a21 = matrix21; a22 = matrix22; a23 = matrix23;
}

void AffineTransform::getLinearPart(double& a11, double& a12, double& a21, double& a22) const {
	a11 = matrix11; a12 = matrix12;
	a21 = matrix21; a22 = matrix22;
}

void AffineTransform::getTranslation(double& tx, double& ty) const {
	tx = matrix13; ty = matrix23;
}

std::string AffineTransform::toString() const {
	std::stringstream ss;
	ss << "AffineTransform(["
		<< matrix11 << ", " << matrix12 << ", " << matrix13 << "], ["
		<< matrix21 << ", " << matrix22 << ", " << matrix23 << "])";

	return ss.str();
}

std::string AffineTransform::toMatrixString() const {
	std::stringstream ss;
	ss << "[[" << matrix11 << ", " << matrix12 << ", " << matrix13 << "],\n"
		<< " [" << matrix21 << ", " << matrix22 << ", " << matrix23 << "]]";

	return ss.str();
}

bool AffineTransform::operator==(const AffineTransform& other) const {
	return std::abs(matrix11 - other.matrix11) < 1e-10 &&
		std::abs(matrix12 - other.matrix12) < 1e-10 &&
		std::abs(matrix13 - other.matrix13) < 1e-10 &&
		std::abs(matrix21 - other.matrix21) < 1e-10 &&
		std::abs(matrix22 - other.matrix22) < 1e-10 &&
		std::abs(matrix23 - other.matrix23) < 1e-10;
}

bool AffineTransform::operator!=(const AffineTransform& other) const {
	return !(*this == other);
}

std::ostream& operator<<(std::ostream& output, const AffineTransform& transform) {
	output << transform.toString();
	return output;
}


void CompositeTransformation::addTransformation(std::unique_ptr<PlaneTransformation> t) {
	transforms.push_back(std::move(t));
}

Complex CompositeTransformation::applyTo(const Complex& point) const {
	Complex result = point;
	for (const auto& t : transforms)
		result = t->applyTo(result);

	return result;
}

std::string CompositeTransformation::toString() const {
	std::stringstream ss;
	ss << "Composite[";
	for (size_t i = 0; i < transforms.size(); ++i) {
		if (i > 0) ss << " ∘ ";
		ss << transforms[i]->toString();
	}
	ss << "]";

	return ss.str();
}

bool CompositeTransformation::isIdentity() const {
	for (const auto& t : transforms)
		if (!t->isIdentity()) return false;
	return true;
}

std::unique_ptr<PlaneTransformation> CompositeTransformation::getInverse() const {
	auto inv = std::make_unique<CompositeTransformation>();
	for (auto it = transforms.rbegin(); it != transforms.rend(); ++it)
		inv->addTransformation((*it)->getInverse());
	return inv;
}
