// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/MOIState.h"
#include "BIMSymbolPresetData.generated.h"

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetIDSet
{
	GENERATED_BODY()

	FBIMSymbolPresetIDSet() = default;
	FBIMSymbolPresetIDSet(const std::initializer_list<int32>& IdList)
		: IDs(IdList) { }

	UPROPERTY()
	TSet<int32> IDs;
};

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOIStateData> Members;

	UPROPERTY()
	TArray<FBIMSymbolPresetIDSet> EquivalentIDs;
};
