// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class MODUMATE_API FDrawingDesignerLine
{
public:
	FDrawingDesignerLine() = default;
	FDrawingDesignerLine(FVector& A, const FVector& B, const FVector& Normal = FVector::ZeroVector)
	: P1(A), P2(B), N(Normal)
	{ }

	void Canonicalize();

	bool operator==(const FDrawingDesignerLine& Rhs) const;
	bool operator!=(const FDrawingDesignerLine& Rhs) const;
	bool operator<(const FDrawingDesignerLine& Rhs) const;
	explicit operator bool() const { return bValid; }

	FVector P1 { ForceInitToZero };
	FVector P2 { ForceInitToZero };
	FVector N { ForceInitToZero };
	float Thickness { 1.0f };
	bool bValid { true };

private:
	static bool LessThan(const FVector& A, const FVector& B);	// Lexicographic comparison
};
