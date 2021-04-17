// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/DimensionOffset.h"


FDimensionOffset FDimensionOffset::Positive(EDimensionOffsetType::Positive, 0.0f);
FDimensionOffset FDimensionOffset::Centered(EDimensionOffsetType::Centered, 0.0f);
FDimensionOffset FDimensionOffset::Negative(EDimensionOffsetType::Negative, 0.0f);

FDimensionOffset::FDimensionOffset(EDimensionOffsetType InType, float InCustomValue)
	: Type(InType)
	, CustomValue(InCustomValue)
{
}

bool FDimensionOffset::operator==(const FDimensionOffset& Other) const
{
	return
		(Type == Other.Type) &&
		(CustomValue == Other.CustomValue);
}

bool FDimensionOffset::operator!=(const FDimensionOffset& Other) const
{
	return !(*this == Other);
}

float FDimensionOffset::GetOffsetDistance(float FlipSign, float TargetThickness) const
{
	switch (Type)
	{
	case EDimensionOffsetType::Centered:
		return 0.0f;
	case EDimensionOffsetType::Negative:
		return -0.5f * FlipSign * TargetThickness;
	case EDimensionOffsetType::Positive:
		return 0.5f * FlipSign * TargetThickness;
	case EDimensionOffsetType::Custom:
		return FlipSign * CustomValue;
	default:
		ensureAlways(false);
		return 0.0f;
	}
}

EDimensionOffsetType FDimensionOffset::GetNextType(int32 Delta, float FlipSign) const
{
	if (Delta == 0.0f)
	{
		return Type;
	}

	bool bPositive = (FlipSign * Delta > 0);
	switch (Type)
	{
	case EDimensionOffsetType::Centered:
		return bPositive ? EDimensionOffsetType::Positive : EDimensionOffsetType::Negative;
	case EDimensionOffsetType::Negative:
		return bPositive ? EDimensionOffsetType::Centered : EDimensionOffsetType::Negative;
	case EDimensionOffsetType::Positive:
		return bPositive ? EDimensionOffsetType::Positive : EDimensionOffsetType::Centered;
	case EDimensionOffsetType::Custom:
		return bPositive ?
			((CustomValue < 0.0f) ? EDimensionOffsetType::Centered : EDimensionOffsetType::Positive) :
			((CustomValue > 0.0f) ? EDimensionOffsetType::Centered : EDimensionOffsetType::Negative);
	default:
		return Type;
	}
}

void FDimensionOffset::Invert()
{
	switch (Type)
	{
	case EDimensionOffsetType::Negative:
		Type = EDimensionOffsetType::Positive;
		break;
	case EDimensionOffsetType::Positive:
		Type = EDimensionOffsetType::Negative;
		break;
	case EDimensionOffsetType::Custom:
		CustomValue *= -1.0f;
		break;
	default:
		break;
	}
}
