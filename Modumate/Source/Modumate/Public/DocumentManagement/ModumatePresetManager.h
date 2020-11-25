// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
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

	//When an assembly can't be found, use a default
	TMap<EObjectType, FBIMAssemblySpec> DefaultAssembliesByObjectType;

public:
	TMap<EObjectType, FAssemblyDataCollection> AssembliesByObjectType;

	FPresetManager();
	virtual ~FPresetManager();

	// DDL 2.0
	FBIMPresetCollection CraftingNodePresets;

	bool TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FBIMKey& PresetID, FBIMAssemblySpec& OutAssembly) const;
	bool TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const;

	EBIMResult GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const;
	EBIMResult RemoveProjectAssemblyForPreset(const FBIMKey& PresetID);
	EBIMResult UpdateProjectAssembly(const FBIMAssemblySpec& Assembly);

	EBIMResult AddOrUpdateGraph2DRecord(const FBIMKey& Key, const FGraph2DRecord& Graph, FBIMKey& OutKey);
	EBIMResult RemoveGraph2DRecord(const FBIMKey& Key);
	EBIMResult GetGraph2DRecord(const FBIMKey& Key, FGraph2DRecord& OutGraph) const;

	EBIMResult FromDocumentRecord(const FModumateDatabase& InDB, const FMOIDocumentRecord& DocRecord);
	EBIMResult ToDocumentRecord(FMOIDocumentRecord& DocRecord) const;

	// Intended as a general way to generate keys: given a base name, append with increment integers until an unused name is found
	// This pattern is used through out the app, particularly in FModumateDocument, to generate IDs for new objects
	// All such schemes should switch to this one key store
	// TODO: the key store is likely to grow, so we may need to add some scope discrimination or a more complex scheme down the road
	FBIMKey GetAvailableKey(const FBIMKey& BaseKey);

	const FBIMAssemblySpec* GetAssemblyByKey(EToolMode Mode, const FBIMKey& Key) const;

	EBIMResult GetAvailablePresetsForSwap(const FBIMKey& ParentPresetID, const FBIMKey& PresetIDToSwap, TArray<FBIMKey>& OutAvailablePresets);
};
