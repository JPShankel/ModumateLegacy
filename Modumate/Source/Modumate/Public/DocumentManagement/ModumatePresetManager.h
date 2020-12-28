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

	bool TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FGuid& PresetID, FBIMAssemblySpec& OutAssembly) const;
	bool TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const;

	EBIMResult GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const;
	EBIMResult RemoveProjectAssemblyForPreset(const FGuid& PresetID);
	EBIMResult UpdateProjectAssembly(const FBIMAssemblySpec& Assembly);

	EBIMResult FromDocumentRecord(const FModumateDatabase& InDB, const FMOIDocumentRecord& DocRecord);
	EBIMResult ToDocumentRecord(FMOIDocumentRecord& DocRecord) const;

	// Intended as a general way to generate keys: given a base name, append with increment integers until an unused name is found
	// This pattern is used through out the app, particularly in UModumateDocument, to generate IDs for new objects
	// All such schemes should switch to this one key store
	// TODO: the key store is likely to grow, so we may need to add some scope discrimination or a more complex scheme down the road
	FBIMKey GetAvailableKey(const FGuid& BaseKey);

	const FBIMAssemblySpec* GetAssemblyByGUID(EToolMode Mode, const FGuid& Key) const;

	EBIMResult GetAvailablePresetsForSwap(const FGuid& ParentPresetID, const FGuid& PresetIDToSwap, TArray<FGuid>& OutAvailablePresets);
};
