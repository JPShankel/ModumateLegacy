#pragma once
#include "CustomDataWebConvertable.h"
#include "BIMNamedDimension.generated.h"

USTRUCT()
struct FBIMNamedDimension : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FString DimensionKey;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	float DefaultValue = 0.0f;

	UPROPERTY()
	FString UIGroup;

	UPROPERTY()
	FString Description;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Dimension);
	};

	friend bool operator==(const FBIMNamedDimension& Lhs, const FBIMNamedDimension& RHS)
	{
		return Lhs.DimensionKey == RHS.DimensionKey
			&& Lhs.DisplayName == RHS.DisplayName
			&& Lhs.DefaultValue == RHS.DefaultValue
			&& Lhs.UIGroup == RHS.UIGroup
			&& Lhs.Description == RHS.Description;
	}

	friend bool operator!=(const FBIMNamedDimension& Lhs, const FBIMNamedDimension& RHS)
	{
		return !(Lhs == RHS);
	}

protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};
