// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
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
struct MODUMATE_API FBIMPresetLayerPriority
{
	GENERATED_BODY()

	UPROPERTY()
	EBIMPresetLayerPriorityGroup PriorityGroup = EBIMPresetLayerPriorityGroup::Other;

	UPROPERTY()
	int32 PriorityValue = 0;

	EBIMResult SetFormElements(FBIMPresetForm& OutForm) const;
};

bool operator==(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator!=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator<(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator>(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator<=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
bool operator>=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS);
