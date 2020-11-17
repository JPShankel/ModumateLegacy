// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

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

	EBIMResult GetDependentPresets(const FBIMKey &PresetID, TArray<FBIMKey>& OutPresets) const;

	EBIMResult GetPropertyFormForPreset(const FBIMKey &PresetID, TMap<FString, FBIMNameType> &OutForm) const;

	EBIMResult LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages);
	EBIMResult CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FBIMKey& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec);
};
