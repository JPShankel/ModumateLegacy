// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Presets/CustomData/CustomDataWebConvertable.h"
#include "QuantitiesDimensions.generated.h"

UENUM()
enum class EQuantitiesDimensions
{
	Count = 0x1,
	Linear = 0x2,
	Area = 0x4,
	Volume= 0x8
};

USTRUCT()
struct MODUMATE_API FBIMConstructionCost : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	float MaterialCostRate = 0.0f;

	UPROPERTY()
	float LaborCostRate = 0.0f;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::ConstructionCost);
	};

protected:

	virtual UStruct* VirtualizedStaticStruct() override
	{
		return FBIMConstructionCost::StaticStruct();
	}
};
