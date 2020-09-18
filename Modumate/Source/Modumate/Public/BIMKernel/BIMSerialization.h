// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMKey.h"
#include "BIMSerialization.generated.h"

USTRUCT()
struct MODUMATE_API FBIMPropertySheetRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FString> Properties;

	UPROPERTY()
	TArray<FName> BindingSources;

	UPROPERTY()
	TArray<FName> BindingTargets;
};

USTRUCT()
struct MODUMATE_API FCraftingPresetRecord
{
	GENERATED_BODY();

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString CategoryTitle;

	UPROPERTY()
	FName NodeType;

	UPROPERTY()
	FBIMKey PresetID;

	UPROPERTY()
	FBIMPropertySheetRecord PropertyRecord;

	UPROPERTY()
	TArray<int32> ChildSetIndices;

	UPROPERTY()
	TArray<int32> ChildSetPositions;

	UPROPERTY()
	TArray<FBIMKey> ChildPresets;

	UPROPERTY()
	TArray<FString> ParentTagPaths;

	UPROPERTY()
	FString MyTagPath;

	UPROPERTY()
	FBIMKey SlotConfigPresetID;

	UPROPERTY()
	TArray<FBIMKey> PartIDs;

	UPROPERTY()
	TArray<FBIMKey> PartParentIDs;

	UPROPERTY()
	TArray<FBIMKey> PartPresets;

	UPROPERTY()
	TArray<FBIMKey> PartSlotNames;
};

USTRUCT()
struct MODUMATE_API FCustomAssemblyCraftingNodeRecord
{
	GENERATED_BODY();

	UPROPERTY()
	FName TypeName;

	UPROPERTY()
	FBIMKey PresetID;

	UPROPERTY()
	int32 InstanceID;

	UPROPERTY()
	int32 ParentID;

	UPROPERTY()
	FBIMPropertySheetRecord PropertyRecord;

	UPROPERTY()
	int32 PinSetIndex;

	UPROPERTY()
	int32 PinSetPosition;
};

