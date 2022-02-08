// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorTypes.h"

class MODUMATE_API FDrawingDesignerLine
{
public:
	FDrawingDesignerLine() = default;
	FDrawingDesignerLine(const FVector& A, const FVector& B, const FVector& Normal = FVector::ZeroVector)
	: P1(A), P2(B), N(Normal)
	{ }

	void Canonicalize();
	float Length() const;

	bool operator==(const FDrawingDesignerLine& Rhs) const;
	bool operator!=(const FDrawingDesignerLine& Rhs) const;
	bool operator<(const FDrawingDesignerLine& Rhs) const;
	explicit operator bool() const { return bValid; }

	FVector P1 { ForceInitToZero };
	FVector P2 { ForceInitToZero };
	FVector N { ForceInitToZero };
	float Thickness { 1.0f };
	mutable bool bValid { true };

private:
	static bool LessThan(const FVector& A, const FVector& B);	// Lexicographic comparison
};

class MODUMATE_API FDrawingDesignerLined
{
public:
	FDrawingDesignerLined() = default;
	FDrawingDesignerLined(const FVector& A, const FVector& B, const FVector& Normal = FVector::ZeroVector)
	: P1(A), P2(B), N(Normal)
	{ }
	FDrawingDesignerLined(const FVector3d& A, const FVector3d& B, const FVector3d& Normal = FVector3d::Zero())
		: P1(A), P2(B), N(Normal)
	{ }

	void Canonicalize();
	double Length() const;

	bool operator==(const FDrawingDesignerLined& Rhs) const;
	bool operator!=(const FDrawingDesignerLined& Rhs) const;
	bool operator<(const FDrawingDesignerLined& Rhs) const;
	explicit operator bool() const { return bValid; }
	operator FDrawingDesignerLine() const;

	FVector3d P1;
	FVector3d P2;
	FVector3d N;
	double Thickness { 1.0f };
	mutable bool bValid { true };

private:
	static bool LessThan(const FVector3d& A, const FVector3d& B);	// Lexicographic comparison
};
