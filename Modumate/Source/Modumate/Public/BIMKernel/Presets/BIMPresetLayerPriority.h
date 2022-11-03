// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "CustomData/CustomDataWebConvertable.h"
#include "ModumateCore/EnumHelpers.h"
#include "BIMPresetLayerPriority.generated.h"

UENUM()
enum class EBIMPresetLayerPriorityGroup : uint8
{
	Structure,
	Substrate,
	Insulation,
	Membrane,
	Carpentry,
	Finish,
	Void,
	Other
};

struct FBIMPresetForm;

USTRUCT()
struct MODUMATE_API FBIMPresetLayerPriority : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	EBIMPresetLayerPriorityGroup PriorityGroup = EBIMPresetLayerPriorityGroup::Other;

	UPROPERTY()
	int32 PriorityValue = 0;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::MiterPriority);
	};

	virtual void ConvertToWebPreset(FBIMWebPreset& OutPreset) override;
	virtual void ConvertFromWebPreset(const FBIMWebPreset& InPreset) override;

	virtual UStruct* VirtualizedStaticStruct() override
	{
		return FBIMPresetLayerPriority::StaticStruct();
	}

};

bool operator==(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator!=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator<(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator>(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator<=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator>=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
