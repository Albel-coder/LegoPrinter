#pragma once
#include "Complex.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <tuple>

// Base class for all plane transformations
class PlaneTransformation
{
public:
	virtual ~PlaneTransformation() = default;
	virtual Complex applyTo(const Complex& point) const = 0;
	virtual std::string toString() const = 0;
	virtual bool isIdentity() const = 0;
	virtual std::unique_ptr<PlaneTransformation> getInverse() const = 0;
	virtual std::unique_ptr<PlaneTransformation> composeWith(const PlaneTransformation& other) const = 0;
};

class Homothety : public PlaneTransformation
{
private:
	Complex center;
	double coefficient;

public:
	Homothety(const Complex& complexNumber = Complex(0.0, 0.0), double k = 1.0);
	Homothety(double centerX, double centerY, double k);

	// Basic getters/setters
	Complex getCenter() const;
	double getCoefficient() const;
	void setCenter(const Complex& complexNumber);
	void setCenter(double x, double y);
	void setCoefficient(double k);

	// Transformation methods
	Complex applyTo(const Complex& point) const override;
	Homothety composeWith(const Homothety& other) const;
	Homothety getInverse();
	Homothety raisedTo(int number) const;

	// New advanced methods
	bool isExpansion() const;      // k > 1
	bool isContraction() const;    // 0 < k < 1
	bool isReflection() const;     // k < 0
	bool isIdentity() const override;

	double getScaleFactor() const;
	Complex getFixedPointers() const; // Returns fixed points

	// Apply to geometric objects
	std::vector<Complex> applyToPolygon(const std::vector<Complex>& polygon) const;
	std::pair<Complex, double> applyToCircle(const Complex& center, double radius) const;

	// Static factory methods
	static Homothety fromScaleFactor(double scale);
	static Homothety fromFixedPointAndImage(const Complex& fixedPoint, const Complex& imagePoint);

	// PlaneTransformation interface
	std::unique_ptr<PlaneTransformation> getInverse() const override;
	std::unique_ptr<PlaneTransformation> composeWith(const PlaneTransformation& other) const override;

	// Operators
	bool operator==(const Homothety& otherNumber) const;
	bool operator!=(const Homothety& otherNumber) const;
	Homothety operator*(const Homothety& otherNumber) const;

	std::string toString() const override;

	static Homothety fromTwoPairs(const Complex& a, const Complex& a1,
		const Complex& b, const Complex& b1);
};

std::ostream& operator<<(std::ostream& output, const Homothety& h);

class AffineTransform : public PlaneTransformation
{
private:
	double matrix11;
	double matrix12;
	double matrix13;
	double matrix21;
	double matrix22;
	double matrix23;

public:
	AffineTransform(double matrix11Value = 1.0, double matrix12Value = 0.0, double matrix13Value = 0.0,
		double matrix21Value = 0.0, double matrix22Value = 1.0, double matrix23Value = 0.0);

	// Static factory methods
	static AffineTransform identity();
	static AffineTransform translation(double tx, double ty);
	static AffineTransform translation(const Complex& vector);
	static AffineTransform scaling(double sx, double sy);
	static AffineTransform scaling(double scale);
	static AffineTransform scaling(const Complex& center, double sx, double sy);
	static AffineTransform rotation(double angleRadius);
	static AffineTransform rotation(double angleRadius, const Complex& center);
	static AffineTransform rotationDegrees(double angleDegrees);
	static AffineTransform rotationDegrees(double angleDegrees, const Complex& center);
	static AffineTransform shear(double shx, double shy);
	static AffineTransform reflectionOverLine(const Complex& firstPoint, const Complex& secondPoint);
	static AffineTransform reflectionOverXAxis();
	static AffineTransform reflectionOverYAxis();
	static AffineTransform projectionOntoLine(const Complex& firstPoint, const Complex& secondPoint);

	// Advanced static constructors
	static AffineTransform fromTriangleMapping(const Complex& srcA, const Complex& srcB, const Complex& srcC,
		const Complex& dstA, const Complex& dstB, const Complex& dstC);
	
	static AffineTransform fromBasisVectors(const Complex& firstE, const Complex& secondE, const Complex& origin = Complex(0.0, 0.0));

	// Transformation methods
	Complex applyTo(const Complex& point) const override;
	AffineTransform composeWith(const AffineTransform& other) const;
	AffineTransform operator*(const AffineTransform& other) const;
	AffineTransform inverse() const;

	// New advanced methods
	bool isIdentity() const override;
	bool isIsometry() const;
	bool isSimilarity() const;    // Keeping the angles
	bool isEquiareal() const;     // We preserve space
	bool isDirect() const;        // Maintaining orientation
	bool isInvolutory() const;    // T^2 = Identity

	// Geometric properties
	Complex getTranslation() const;
	double getRotationAngle() const;
	Complex getScaleFactors() const;
	Complex getShearFactors() const;
	double getAreaScale() const;     // Area change factor

	// Fixed points and invariant lines
	std::vector<Complex> getFixedPoints() const;
	bool hasFixedPoint(const Complex& point) const;

	// Eigen analysis
	std::vector<Complex> getEigenvalues() const;
	std::vector<Complex> getEigenvectors() const;

	// Apply to geometric objects
	std::vector<Complex> applyToPolygon(const std::vector<Complex>& polygon) const;
	std::pair<Complex, double> applyToCircle(const Complex& center, double radius) const;
	Complex applyToVector(const Complex& vector) const; // No broadcast

	// Decomposition methods
	void decompose(Complex& translation, Complex& scale, double& rotationAngle) const;
	std::tuple<AffineTransform, AffineTransform, AffineTransform> decomposeLDU() const; // LDU decomposition
	std::tuple<AffineTransform, AffineTransform, AffineTransform> decomposeQR() const; // QR decomposition

	// Matrix operations
	AffineTransform transpose() const;
	double trace() const;
	double determinant() const;
	AffineTransform adjugate() const;

	// Power and exponential
	AffineTransform power(int number) const;
	AffineTransform exponential() const;

	// Interpolation
	static AffineTransform lerp(const AffineTransform& a, const AffineTransform& b, double t);

	// PlaneTransformation interface
	std::unique_ptr<PlaneTransformation> getInverse() const override;
	std::unique_ptr<PlaneTransformation> composeWith(const PlaneTransformation& other) const override;

	// Getters
	void getMatrix(double& matrix11Value, double& matrix12Value, double& matrix13Value,
		double& matrix21Value, double& matrix22Value, double& matrix23Value) const;
	void getLinearPart(double& matrix11Value, double& matrix12Value, double& matrix21Value, double& matrix22Value) const;
	void getTranslation(double& tx, double& ty) const;

	std::string toString() const override;
	std::string toMatrixString() const;

	bool operator==(const AffineTransform& other) const;
	bool operator!=(const AffineTransform& other) const;

	};

	class CompositeTransformation : public PlaneTransformation {
	private:
		std::vector<std::unique_ptr<PlaneTransformation>> transforms;
	public:
		void addTransformation(std::unique_ptr<PlaneTransformation> t);
		Complex applyTo(const Complex& point) const override;
		std::string toString() const override;
		bool isIdentity() const override;
		std::unique_ptr<PlaneTransformation> getInverse() const override;
		std::unique_ptr<PlaneTransformation> composeWith(const PlaneTransformation& other) const override;
	};

	std::ostream& operator<<(std::ostream& output, const AffineTransform& transform);