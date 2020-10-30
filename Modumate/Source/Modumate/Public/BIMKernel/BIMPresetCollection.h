// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "BIMKernel/BIMPresetInstance.h"
#include "BIMKernel/BIMKey.h"
#include "BIMKernel/BIMProperties.h"

#include "CoreMinimal.h"

#include "BIMPresetCollection.generated.h"

USTRUCT()
struct MODUMATE_API FBIMPresetCollection
{
	GENERATED_BODY()
	
	UPROPERTY()
	TMap<FName, FBIMPresetTypeDefinition> NodeDescriptors;

	UPROPERTY()
	TMap<FBIMKey, FBIMPresetInstance> Presets;

	EObjectType GetPresetObjectType(const FBIMKey &PresetID) const;

	EBIMResult ToDataRecords(TArray<FCraftingPresetRecord> &OutRecords) const;
	EBIMResult FromDataRecords(const TArray<FCraftingPresetRecord> &Record);

	EBIMResult GetDependentPresets(const FBIMKey &PresetID, TArray<FBIMKey>& OutPresets) const;

	EBIMResult GetPropertyFormForPreset(const FBIMKey &PresetID, TMap<FString, FBIMNameType> &OutForm) const;

	EBIMResult LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages);
	EBIMResult CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FBIMKey& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec);
};
