// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

#include "BIMPresetCollection.generated.h"

static constexpr int32 BIMPresetCollectionCurrentVersion = 3;

USTRUCT()
struct MODUMATE_API FBIMPresetCollection
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = BIMPresetCollectionCurrentVersion;
	
	UPROPERTY()
	TMap<FName, FBIMPresetTypeDefinition> NodeDescriptors;

	UPROPERTY()
	TMap<FBIMKey, FBIMPresetInstance> Presets;

	TMap<FGuid, FBIMKey> GUIDKeyMap;

	TSet<FGuid> UsedGUIDs;

	TSet<FBIMTagPath> AllNCPs;

	EBIMResult PostLoad();

	bool Matches(const FBIMPresetCollection& OtherCollection) const;

	EObjectType GetPresetObjectType(const FBIMKey& PresetID) const;

	EBIMResult GetNCPForPreset(const FBIMKey& InPresetID, FBIMTagPath& OutNCP) const;
	EBIMResult GetPresetsForNCP(const FBIMTagPath& InNCP, TArray<FBIMKey>& OutPresets) const;
	EBIMResult GetNCPSubcategories(const FBIMTagPath& InNCP, TArray<FString>& OutSubcats) const;

	EBIMResult GetDependentPresets(const FBIMKey& PresetID, TArray<FBIMKey>& OutPresets) const;

	EBIMResult GetPropertyFormForPreset(const FBIMKey& PresetID, TMap<FString, FBIMNameType> &OutForm) const;

	EBIMResult GetPresetsByPredicate(const TFunction<bool(const FBIMPresetInstance& Preset)>& Predicate,TArray<FBIMKey>& OutPresets) const;
	EBIMResult GetPresetsForSlot(const FBIMKey& SlotPresetID,TArray<FBIMKey>& OutPresets) const;

	EBIMResult GenerateBIMKeyForPreset(const FBIMKey& PresetID, FBIMKey& OutKey) const;
	EBIMResult GetAvailableGUID(FGuid& OutGUID);

	EBIMResult LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages);
	EBIMResult CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FBIMKey& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec);
};
