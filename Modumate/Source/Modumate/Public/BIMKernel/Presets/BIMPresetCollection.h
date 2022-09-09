// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetNCPTaxonomy.h"
#include "DocumentManagement/DocumentDelta.h"

#include "BIMPresetCollection.generated.h"

static constexpr int32 BIMPresetCollectionCurrentVersion = 8;
// Version 5 - FBIMPresetForm & BIM deltas
// Version 6 - Taxonomy added to collection
// Version 7 - Point hosted objects
// Version 8 - Construction details as custom properties

struct FBIMPresetDelta;

// Can't include ModumateSerialization, circular dependency
struct FMOIDocumentRecordV5;
using FMOIDocumentRecord = FMOIDocumentRecordV5;

USTRUCT()
struct MODUMATE_API FBIMWebPresetCollection
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGuid, FBIMWebPreset> presets;

	UPROPERTY()
	TArray<FBIMPresetTaxonomyNode> ncpTaxonomy;
};

UENUM()
enum class EWebPresetChangeVerb
{
	Add,
	Update,
	Remove
};

USTRUCT()
struct MODUMATE_API FWebPresetChangePackage
{
	GENERATED_BODY()

	UPROPERTY()
	EWebPresetChangeVerb verb;

	UPROPERTY()
	TMap<FGuid, FBIMWebPreset> presets;
};

USTRUCT()
struct MODUMATE_API FBIMPresetCollection
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = BIMPresetCollectionCurrentVersion;
	
	UPROPERTY()
	TMap<FName, FBIMPresetTypeDefinition> NodeDescriptors;

	UPROPERTY()
	TMap<FGuid, FBIMPresetInstance> PresetsByGUID;

	// Not a UPROPERTY because we don't want it to serialize in saved games
	// Copied from the object database for convenience
	FBIMPresetNCPTaxonomy PresetTaxonomy;

	FBIMPresetInstance* PresetFromGUID(const FGuid& InGUID);
	const FBIMPresetInstance* PresetFromGUID(const FGuid& InGUID) const;

	EBIMResult AddPreset(const FBIMPresetInstance& InPreset);
	EBIMResult RemovePreset(const FGuid& InGUID);

	//When an assembly can't be found, use a default
	TMap<EObjectType, FBIMAssemblySpec> DefaultAssembliesByObjectType;
	TMap<EObjectType, FAssemblyDataCollection> AssembliesByObjectType;
	TSet<FGuid> UsedGUIDs;
	TSet<FBIMTagPath> AllNCPs;

	EBIMResult PostLoad();
	EBIMResult ProcessNamedDimensions();
	EBIMResult ProcessStarterAssemblies(const FModumateDatabase& AssetDatabase, const TArray<FGuid>& StarterPresets);
	EBIMResult SetPartSizesFromMeshes();

	EObjectType GetPresetObjectType(const FGuid& PresetID) const;

	EBIMResult GetNCPForPreset(const FGuid& InPresetID, FBIMTagPath& OutNCP) const;
	EBIMResult GetPresetsForNCP(const FBIMTagPath& InNCP, TArray<FGuid>& OutPresets, bool bExactMatch = false) const;
	EBIMResult GetNCPSubcategories(const FBIMTagPath& InNCP, TArray<FString>& OutSubcats) const;

	EBIMResult GetAllDescendentPresets(const FGuid& PresetGUID, TArray<FGuid>& OutPresets) const;
	EBIMResult GetAllAncestorPresets(const FGuid& PresetGUID, TArray<FGuid>& OutPresets) const;

	EBIMResult GetDescendentPresets(const FGuid& InPresetGUID, TArray<FGuid>& OutPresets) const;
	EBIMResult GetAncestorPresets(const FGuid& InPresetGUID, TArray<FGuid>& OutPresets) const;

	EBIMResult GetPresetsByPredicate(const TFunction<bool(const FBIMPresetInstance& Preset)>& Predicate,TArray<FGuid>& OutPresets) const;
	EBIMResult GetPresetsForSlot(const FGuid& SlotPresetGUID,TArray<FGuid>& OutPresets) const;

	EBIMResult GenerateBIMKeyForPreset(const FGuid& PresetID, FBIMKey& OutKey) const;
	EBIMResult GetAvailableGUID(FGuid& OutGUID);

	EBIMResult LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FGuid>& OutStarters, TArray<FString>& OutMessages);

	EBIMResult ForEachPreset(const TFunction<void(const FBIMPresetInstance& Preset)>& Operation) const;
	EBIMResult GetAvailablePresetsForSwap(const FGuid& ParentPresetID, const FGuid& PresetIDToSwap, TArray<FGuid>& OutAvailablePresets) const;

	EBIMResult GetBlankPresetForObjectType(EObjectType ObjectType, FBIMPresetInstance& OutPreset) const;

	TSharedPtr<FBIMPresetDelta> MakeUpdateDelta(const FBIMPresetInstance& UpdatedPreset, UObject* AnalyticsWorldContextObject = nullptr) const;
	TSharedPtr<FBIMPresetDelta> MakeUpdateDelta(const FGuid& PresetID, UObject* AnalyticsWorldContextObject = nullptr) const;
	TSharedPtr<FBIMPresetDelta> MakeCreateNewDelta(FBIMPresetInstance& NewPreset, UObject* AnalyticsWorldContextObject = nullptr);
	TSharedPtr<FBIMPresetDelta> MakeDuplicateDelta(const FGuid& OriginalID, FBIMPresetInstance& NewPreset, UObject* AnalyticsWorldContextObject = nullptr);
	EBIMResult MakeDeleteDeltas(const FGuid& DeleteGUID, const FGuid& ReplacementGUID, TArray<FDeltaPtr>& OutDeltas, UObject* AnalyticsWorldContextObject = nullptr);

	// Datasmith
	EBIMResult MakeNewPresetFromDatasmith(const FModumateDatabase& AssetDatabase, const FString& NewPresetName, const FGuid& ArchitecturalMeshID, FGuid& OutPresetID);

	bool TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FGuid& PresetID, FBIMAssemblySpec& OutAssembly) const;
	bool TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const;

	EBIMResult GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const;
	EBIMResult RemoveProjectAssemblyForPreset(const FGuid& PresetID);
	EBIMResult UpdateProjectAssembly(const FBIMAssemblySpec& Assembly);
	const FBIMAssemblySpec* GetAssemblyByGUID(EToolMode Mode, const FGuid& Key) const;

	bool SavePresetsToDocRecord(FMOIDocumentRecord& DocRecord) const;
	bool ReadPresetsFromDocRecord(const FModumateDatabase& InDB, int32 DocRecordVersion, const FMOIDocumentRecord& DocRecord);

	EBIMResult GetWebPresets(FBIMWebPresetCollection& OutPresets, UWorld* World);

	bool operator==(const FBIMPresetCollection& RHS) const;
	bool operator!=(const FBIMPresetCollection& RHS) const;
};

class MODUMATE_API FBIMPresetCollectionProxy
{
private:
	const FBIMPresetCollection* BaseCollection = nullptr;
	TMap<FGuid, FBIMPresetInstance> OverriddenPresets;

public:
	FBIMPresetCollectionProxy();
	FBIMPresetCollectionProxy(const FBIMPresetCollection& InCollection);
	const FBIMPresetInstance* PresetFromGUID(const FGuid& InGUID) const;
	const FBIMAssemblySpec* AssemblySpecFromGUID(EObjectType ObjectType,const FGuid& InGUID) const;

	EBIMResult CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FGuid& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec);

	EBIMResult OverridePreset(const FBIMPresetInstance& PresetInstance);
};