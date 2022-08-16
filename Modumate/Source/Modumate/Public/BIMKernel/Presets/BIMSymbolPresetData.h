// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/MOIState.h"
#include "BIMSymbolPresetData.generated.h"

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetIDSet
{
	GENERATED_BODY()

	UPROPERTY()
	TSet<int32> IDSet;
};

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetData
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<int32, FMOIStateData> Members;

	UPROPERTY()
	TMap<int32, FBIMSymbolPresetIDSet> EquivalentIDs;
};
