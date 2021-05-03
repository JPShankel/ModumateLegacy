// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMPresetLayerPriority.generated.h"

UENUM()
enum class EBIMPresetLayerPriorityGroup
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
	EBIMPresetLayerPriorityGroup PriorityGroup;

	UPROPERTY()
	int32 PriorityValue;

	EBIMResult SetFormElements(FBIMPresetForm& OutForm) const;
};
