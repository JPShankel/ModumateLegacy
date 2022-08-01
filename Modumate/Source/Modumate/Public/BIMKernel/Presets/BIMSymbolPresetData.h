// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/MOIState.h"
#include "BIMSymbolPresetData.generated.h"

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOIStateData> Members;
};
