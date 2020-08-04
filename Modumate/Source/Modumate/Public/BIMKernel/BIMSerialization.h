// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
	FName NodeType;

	UPROPERTY()
	FName PresetID;

	UPROPERTY()
	FBIMPropertySheetRecord PropertyRecord;

	UPROPERTY()
	TArray<int32> ChildSetIndices;

	UPROPERTY()
	TArray<int32> ChildSetPositions;

	UPROPERTY()
	TArray<FName> ChildPresets;

	UPROPERTY()
	TArray<FString> ParentTagPaths;

	UPROPERTY()
	FString MyTagPath;
};

USTRUCT()
struct MODUMATE_API FCustomAssemblyCraftingNodeRecord
{
	GENERATED_BODY();

	UPROPERTY()
	FName TypeName;

	UPROPERTY()
	FName PresetID;

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
