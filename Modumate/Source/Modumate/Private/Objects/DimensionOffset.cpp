// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/DimensionOffset.h"
#include "ModumateCore/PrettyJSONWriter.h"

FDimensionOffset FDimensionOffset::Positive(EDimensionOffsetType::Positive, 0.0f);
FDimensionOffset FDimensionOffset::Centered(EDimensionOffsetType::Centered, 0.0f);
FDimensionOffset FDimensionOffset::Negative(EDimensionOffsetType::Negative, 0.0f);

FDimensionOffset::FDimensionOffset(EDimensionOffsetType InType, float InCustomValue)
	: type(InType)
	, customValue(InCustomValue)
{
}

bool FDimensionOffset::operator==(const FDimensionOffset& Other) const
{
	return
		(type == Other.type) &&
		(customValue == Other.customValue);
}

bool FDimensionOffset::operator!=(const FDimensionOffset& Other) const
{
	return !(*this == Other);
}

float FDimensionOffset::GetOffsetDistance(float FlipSign, float TargetThickness) const
{
	switch (type)
	{
	case EDimensionOffsetType::Centered:
		return 0.0f;
	case EDimensionOffsetType::Negative:
		return -0.5f * FlipSign * TargetThickness;
	case EDimensionOffsetType::Positive:
		return 0.5f * FlipSign * TargetThickness;
	case EDimensionOffsetType::Custom:
		return FlipSign * customValue;
	default:
		ensureAlways(false);
		return 0.0f;
	}
}

EDimensionOffsetType FDimensionOffset::GetNextType(int32 Delta, float FlipSign) const
{
	if (Delta == 0.0f)
	{
		return type;
	}

	bool bPositive = (FlipSign * Delta > 0);
	switch (type)
	{
	case EDimensionOffsetType::Centered:
		return bPositive ? EDimensionOffsetType::Positive : EDimensionOffsetType::Negative;
	case EDimensionOffsetType::Negative:
		return bPositive ? EDimensionOffsetType::Centered : EDimensionOffsetType::Negative;
	case EDimensionOffsetType::Positive:
		return bPositive ? EDimensionOffsetType::Positive : EDimensionOffsetType::Centered;
	case EDimensionOffsetType::Custom:
		return bPositive ?
			((customValue < 0.0f) ? EDimensionOffsetType::Centered : EDimensionOffsetType::Positive) :
			((customValue > 0.0f) ? EDimensionOffsetType::Centered : EDimensionOffsetType::Negative);
	default:
		return type;
	}
}

void FDimensionOffset::Invert()
{
	switch (type)
	{
	case EDimensionOffsetType::Negative:
		type = EDimensionOffsetType::Positive;
		break;
	case EDimensionOffsetType::Positive:
		type = EDimensionOffsetType::Negative;
		break;
	case EDimensionOffsetType::Custom:
		customValue *= -1.0f;
		break;
	default:
		break;
	}
}

FString FDimensionOffset::ToString() const
{
	FString outputString;
	WriteJsonGeneric(outputString, this);
	return outputString;
}

void FDimensionOffset::FromString(FString& str)
{
	ReadJsonGeneric(str, this);
}

