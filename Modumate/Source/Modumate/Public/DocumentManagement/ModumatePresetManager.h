// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "BIMKernel/BIMLegacyPortals.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "DocumentManagement/ModumateSerialization.h"

class MODUMATE_API FPresetManager
{

public:
	typedef TModumateDataCollection<FBIMAssemblySpec> FAssemblyDataCollection;
	typedef TMap<FName, FGraph2DRecord> FGraphCollection;

private:

	// FModumateDatabase initializes the preset manager for a new document
	friend class FModumateDatabase;

	FGraphCollection GraphCollection;
	TSet<FName> KeyStore;
	ECraftingResult ReadBIMTable(UDataTable* DataTable, Modumate::BIM::FCraftingPresetCollection& Target);

public:
	TMap<EObjectType, FAssemblyDataCollection> AssembliesByObjectType;

	FPresetManager();
	virtual ~FPresetManager();

	static const int32 MinimumReadableVersion = 7;

	// DDL 2.0
	Modumate::BIM::FCraftingPresetCollection CraftingNodePresets, DraftingNodePresets;
	TMap<EObjectType, FName> StarterPresetsByObjectType;

	ECraftingResult PresetToSpec(const FName& PresetID, FBIMAssemblySpec& OutPropertySpec) const;

	bool TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FName& PresetID, FBIMAssemblySpec& OutAssembly) const;
	bool TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const;

	ECraftingResult GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const;
	ECraftingResult RemoveProjectAssemblyForPreset(const FName& PresetID);
	ECraftingResult UpdateProjectAssembly(const FBIMAssemblySpec& Assembly);

	ECraftingResult AddOrUpdateGraph2DRecord(FName Key, const FGraph2DRecord& Graph, FName& OutKey);
	ECraftingResult RemoveGraph2DRecord(const FName& Key);
	ECraftingResult GetGraph2DRecord(const FName& Key, FGraph2DRecord& OutGraph) const;

	// Intended as a general way to generate keys: given a base name, append with increment integers until an unused name is found
	// This pattern is used through out the app, particularly in FModumateDocument, to generate IDs for new objects
	// All such schemes should switch to this one key store
	// TODO: the key store is likely to grow, so we may need to add some scope discrimination or a more complex scheme down the road
	FName GetAvailableKey(const FName& BaseKey);

	ECraftingResult FromDocumentRecord(UWorld* World, const FModumateDocumentHeader& DocumentHeader, const FMOIDocumentRecord& DocumentRecord);
	ECraftingResult ToDocumentRecord(FMOIDocumentRecord& OutRecord) const;

	const FBIMAssemblySpec* GetAssemblyByKey(EToolMode Mode, const FName& Key) const;
};
