// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

#include "BIMPresetCollection.generated.h"

static constexpr int32 BIMPresetCollectionCurrentVersion = 4;

USTRUCT()
struct MODUMATE_API FBIMPresetCollection
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = BIMPresetCollectionCurrentVersion;
	
	UPROPERTY()
	TMap<FName, FBIMPresetTypeDefinition> NodeDescriptors;

	UPROPERTY()
	TMap<FBIMKey, FBIMPresetInstance> Presets_DEPRECATED;

	UPROPERTY()
	TMap<FGuid, FBIMPresetInstance> PresetsByGUID;

	FBIMPresetInstance* PresetFromGUID(const FGuid& InGUID);
	const FBIMPresetInstance* PresetFromGUID(const FGuid& InGUID) const;

	EBIMResult AddPreset(const FBIMPresetInstance& InPreset);
	EBIMResult RemovePreset(const FGuid& InGUID);

	TSet<FGuid> UsedGUIDs;

	TSet<FBIMTagPath> AllNCPs;

	EBIMResult PostLoad();

	bool Matches(const FBIMPresetCollection& OtherCollection) const;

	EObjectType GetPresetObjectType(const FGuid& PresetID) const;

	EBIMResult GetNCPForPreset(const FGuid& InPresetID, FBIMTagPath& OutNCP) const;
	EBIMResult GetPresetsForNCP(const FBIMTagPath& InNCP, TArray<FGuid>& OutPresets) const;
	EBIMResult GetNCPSubcategories(const FBIMTagPath& InNCP, TArray<FString>& OutSubcats) const;

	EBIMResult GetDependentPresets(const FGuid& PresetGUID, TArray<FGuid>& OutPresets) const;

	EBIMResult GetPropertyFormForPreset(const FGuid& PresetGUID, TMap<FString, FBIMNameType> &OutForm) const;

	EBIMResult GetPresetsByPredicate(const TFunction<bool(const FBIMPresetInstance& Preset)>& Predicate,TArray<FGuid>& OutPresets) const;
	EBIMResult GetPresetsForSlot(const FGuid& SlotPresetGUID,TArray<FGuid>& OutPresets) const;

	EBIMResult GenerateBIMKeyForPreset(const FGuid& PresetID, FBIMKey& OutKey) const;
	EBIMResult GetAvailableGUID(FGuid& OutGUID);

	EBIMResult LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FGuid>& OutStarters, TArray<FString>& OutMessages);
	EBIMResult CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FGuid& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec);
};
