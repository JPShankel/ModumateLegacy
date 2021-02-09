// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DimensionOffset.generated.h"

UENUM()
enum class EDimensionOffsetType : uint8
{
	Centered,
	Negative,
	Positive,
	Custom
};

USTRUCT()
struct MODUMATE_API FDimensionOffset
{
	GENERATED_BODY()

	FDimensionOffset() = default;
	FDimensionOffset(EDimensionOffsetType InType, float InCustomValue);

	UPROPERTY()
	EDimensionOffsetType Type = EDimensionOffsetType::Centered;

	UPROPERTY()
	float CustomValue = 0.0f;

	float GetOffsetDistance(float FlipSign, float TargetThickness) const;
	EDimensionOffsetType GetNextType(int32 Delta, float FlipSign) const;

	static FDimensionOffset Positive;
	static FDimensionOffset Centered;
	static FDimensionOffset Negative;
};
