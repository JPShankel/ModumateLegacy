// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMSerialization.generated.h"

USTRUCT()
struct MODUMATE_API FBIMPropertySheetRecord
{
	GENERATED_USTRUCT_BODY()

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
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FName NodeType;

	UPROPERTY()
	FName PresetID;

	UPROPERTY()
	bool IsReadOnly;

	UPROPERTY()
	FBIMPropertySheetRecord PropertyRecord;

	UPROPERTY()
	TArray<int32> ChildNodePinSetIndices;

	UPROPERTY()
	TArray<int32> ChildNodePinSetPositions;

	UPROPERTY()
	TArray<FName> ChildNodePinSetPresetIDs;

	UPROPERTY()
	TArray<FName> ChildNodePinSetNodeTypes;

	UPROPERTY()
	TArray<FString> ChildNodePinSetTags;

	UPROPERTY()
	TArray<FString> Tags;
};

USTRUCT()
struct FCustomAssemblyCraftingNodeRecord
{
	GENERATED_USTRUCT_BODY();

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
