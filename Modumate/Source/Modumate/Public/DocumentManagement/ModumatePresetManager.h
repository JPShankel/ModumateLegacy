// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMPresets.h"
#include "DocumentManagement/ModumateSerialization.h"

class MODUMATE_API FPresetManager
{

public:
	typedef TModumateDataCollection<FBIMAssemblySpec> FAssemblyDataCollection;
	typedef TMap<FBIMKey, FGraph2DRecord> FGraphCollection;

private:

	// FModumateDatabase initializes the preset manager for a new document
	friend class FModumateDatabase;

	FGraphCollection GraphCollection;
	TSet<FString> KeyStore;

public:
	TMap<EObjectType, FAssemblyDataCollection> AssembliesByObjectType;

	FPresetManager();
	virtual ~FPresetManager();

	static const int32 MinimumReadableVersion = 7;

	// DDL 2.0
	FBIMPresetCollection CraftingNodePresets, DraftingNodePresets;

	bool TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FBIMKey& PresetID, FBIMAssemblySpec& OutAssembly) const;
	bool TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const;

	ECraftingResult GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const;
	ECraftingResult RemoveProjectAssemblyForPreset(const FBIMKey& PresetID);
	ECraftingResult UpdateProjectAssembly(const FBIMAssemblySpec& Assembly);

	ECraftingResult AddOrUpdateGraph2DRecord(const FBIMKey& Key, const FGraph2DRecord& Graph, FBIMKey& OutKey);
	ECraftingResult RemoveGraph2DRecord(const FBIMKey& Key);
	ECraftingResult GetGraph2DRecord(const FBIMKey& Key, FGraph2DRecord& OutGraph) const;

	// Intended as a general way to generate keys: given a base name, append with increment integers until an unused name is found
	// This pattern is used through out the app, particularly in FModumateDocument, to generate IDs for new objects
	// All such schemes should switch to this one key store
	// TODO: the key store is likely to grow, so we may need to add some scope discrimination or a more complex scheme down the road
	FBIMKey GetAvailableKey(const FBIMKey& BaseKey);

	ECraftingResult FromDocumentRecord(UWorld* World, const FModumateDocumentHeader& DocumentHeader, const FMOIDocumentRecord& DocumentRecord);
	ECraftingResult ToDocumentRecord(FMOIDocumentRecord& OutRecord) const;

	const FBIMAssemblySpec* GetAssemblyByKey(EToolMode Mode, const FBIMKey& Key) const;

	ECraftingResult GetAvailablePresetsForSwap(const FBIMKey& ParentPresetID, const FBIMKey& PresetIDToSwap, TArray<FBIMKey>& OutAvailablePresets);
};
