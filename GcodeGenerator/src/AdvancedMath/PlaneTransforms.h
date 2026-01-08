#pragma once
#include "Complex.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

// Base class for all plane transformations
class PlaneTransformation
{
public:
	virtual ~PlaneTransformation() = default;
	virtual Complex ApplyTo(const Complex& Point) const = 0;
	virtual std::string ToString() const = 0;
	virtual bool IsIdentity() const = 0;
	virtual std::unique_ptr<PlaneTransformation> GetInverse() const = 0;
	virtual std::unique_ptr<PlaneTransformation> ComposeWith(const PlaneTransformation& Other) const = 0;
};

class Homothety : public PlaneTransformation
{
private:
	Complex Center;
	double Coefficient;

public:
	Homothety(const Complex& ComplexNumber = Complex(0.0, 0.0), double K = 1.0);
	Homothety(double CenterX, double CenterY, double K);

	// Basic getters/setters
	Complex GetCenter() const;
	double GetCoefficient() const;
	void SetCenter(const Complex& ComplexNumber);
	void SetCenter(double X, double Y);
	void SetCoefficient(double K);

	// Transformation methods
	Complex ApplyTo(const Complex& Point) const override;
	Homothety ComposeWith(const Homothety& Other) const;
	Homothety getInverse() const;
	Homothety RaisedTo(int Number) const;

	// New advanced methods
	bool IsExpansion() const;      // k > 1
	bool IsContraction() const;    // 0 < k < 1
	bool IsReflection() const;     // k < 0
	bool IsIdentity() const override;

	double GetScaleFactor() const;
	Complex GetFixedPointers() const; // Returns fixed points

	// Apply to geometric objects
	std::vector<Complex> ApplyToPolygon(const std::vector<Complex>& Polygon) const;
	std::pair<Complex, double> ApplyToCircle(const Complex& Center, double Radius) const;

	// Static factory methods
	static Homothety FromScaleFactor(double Scale);
	static Homothety FromFixedPointAndImage(const Complex& FixedPoint, const Complex& ImagePoint);

	// PlaneTransformation interface
	std::unique_ptr<PlaneTransformation> GetInverse() const override;
	std::unique_ptr<PlaneTransformation> ComposeWith(const PlaneTransformation& Other) const override;

	// Operators
	bool operator==(const Homothety& OtherNumber) const;
	bool operator!=(const Homothety& OtherNumber) const;
	Homothety operator*(const Homothety& OtherNumber) const;

	std::string ToString() const override;

	static Homothety FromTwoPairs(const Complex& A, const Complex& A1,
		const Complex& B, const Complex& B1);
};

std::ostream& operator<<(std::ostream& Output, const Homothety& H);

class AffineTransform : public PlaneTransformation
{
private:
	double Matrix11;
	double Matrix12;
	double Matrix13;
	double Matrix21;
	double Matrix22;
	double Matrix23;

public:
	AffineTransform(double matrix11 = 1.0, double matrix12 = 0.0, double matrix13 = 0.0,
		double matrix21 = 0.0, double matrix22 = 1.0, double matrix23 = 0.0);

	// Static factory methods
	static AffineTransform Identity();
	static AffineTransform Translation(double tx, double ty);
	static AffineTransform Translation(const Complex& Vector);
	static AffineTransform Scaling(double sx, double sy);
	static AffineTransform Scaling(double Scale);
	static AffineTransform Scaling(const Complex& Center, double sx, double sy);
	static AffineTransform Rotation(double AngleRadius);
	static AffineTransform Rotation(double AngleRadius, const Complex& Center);
	static AffineTransform RotationDegrees(double AngleDegrees);
	static AffineTransform RotationDegrees(double AngleDegrees, const Complex& Center);
	static AffineTransform Shear(double shx, double shy);
	static AffineTransform ReflectionOverLine(const Complex& FirstPoint, const Complex& SecondPoint);
	static AffineTransform ReflectionOverXAxis();
	static AffineTransform ReflectionOverYAxis();
	static AffineTransform ProjectionOntoLine(const Complex& FirstPoint, const Complex& SecondPoint);

	// Advanced static constructors
	static AffineTransform FromTriangleMapping(const Complex& srcA, const Complex& srcB, const Complex& srcC,
		const Complex& dstA, const Complex& dstB, const Complex& dstC);
	
	static AffineTransform FromBasisVectors(const Complex& FirstE, const Complex& SecondE, const Complex& Origin = Complex(0.0, 0.0));

	// Transformation methods
	Complex ApplyTo(const Complex& Point) const override;
	AffineTransform ComposeWith(const AffineTransform& Other) const;
	AffineTransform operator*(const AffineTransform& Other) const;
	AffineTransform Inverse() const;

	// New advanced methods
	bool IsIdentity() const override;
	bool IsIsometry() const;
	bool IsSimilarity() const;    // Keeping the angles
	bool IsEquiareal() const;     // We preserve space
	bool IsDirect() const;        // Maintaining orientation
	bool IsInvolutory() const;    // T^2 = Identity

	// Geometric properties
	Complex GetTranslation() const;
	double GetRotationAngle() const;
	Complex GetScaleFactors() const;
	Complex GetShearFactors() const;
	double GetAreaScale() const;     // Area change factor

	// Fixed points and invariant lines
	std::vector<Complex> GetFixedPoints() const;
	bool HasFixedPoint(const Complex& Point) const;

	// Eigen analysis
	std::vector<Complex> GetEigenvalues() const;
	std::vector<Complex> GetEigenvectors() const;

	// Apply to geometric objects
	std::vector<Complex> ApplyToPolygon(const std::vector<Complex>& Polygon) const;
	std::pair<Complex, double> ApplyToCircle(const Complex& Center, double Radius) const;
	Complex ApplyToVector(const Complex& Vector) const; // No broadcast

	// Decomposition methods
	void Decompose(Complex& Translation, Complex& Scale, double RotationAngle) const;
	std::tuple<AffineTransform, AffineTransform, AffineTransform> DecomposeLDU() const; // LDU decomposition
	std::tuple<AffineTransform, AffineTransform, AffineTransform> DecomposeQR() const; // QR decomposition

	// Matrix operations
	AffineTransform Transpose() const;
	double Trace() const;
	double Determinant() const;
	AffineTransform Adjugate() const;

	// Power and exponential
	AffineTransform Power(int Number) const;
	AffineTransform Exponential() const;

	// Interpolation
	static AffineTransform Lerp(const AffineTransform& A, const AffineTransform& B, const AffineTransform& C);

	// PlaneTransformation interface
	std::unique_ptr<PlaneTransformation> GetInverse() const override;
	std::unique_ptr<PlaneTransformation> ComposeWith(const PlaneTransformation& Other) const override;

	// Getters
	void GetMatrix(double& Matrix11, double& Matrix12, double& Matrix13,
		double& Matrix21, double& Matrix22, double& Matrix23) const;
	void GetLinearPart(double& Matrix11, double& Matrix12, double& Matrix21, double& Matrix22) const;
	void GetTranslation(double& tx, double& ty) const;

	std::string ToString() const override;
	std::string ToMatrixString() const;

	bool operator==(const AffineTransform& Other) const;
	bool operator!=(const AffineTransform& Other) const;

	};

	std::ostream& operator<<(std::ostream& Output, const AffineTransform& Transform);