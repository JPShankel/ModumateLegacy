// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "BIMKernel/BIMLegacyPortals.h"
#include "Database/ModumateObjectAssembly.h"
#include "DocumentManagement/ModumateSerialization.h"

namespace Modumate
{
	class MODUMATE_API FPresetManager 
	{
	private:

		// ModumateObjectDatabase initializes the preset manager for a new document
		friend class ModumateObjectDatabase;

		typedef DataCollection<FModumateObjectAssembly> FAssemblyDataCollection;
		TMap<EObjectType, FAssemblyDataCollection> AssembliesByObjectType;

		typedef TMap<FName, FGraph2DRecord> FGraphCollection;
		FGraphCollection GraphCollection;

		TSet<FName> KeyStore;

		ECraftingResult ReadBIMTable(UDataTable *DataTable, BIM::FCraftingPresetCollection &Target);

	public:
		TMap<EToolMode, FAssemblyDataCollection > AssemblyDBs_DEPRECATED;

		FPresetManager();
		virtual ~FPresetManager();

		static const int32 MinimumReadableVersion = 7;

		// DDL 2.0
		BIM::FCraftingPresetCollection CraftingNodePresets,DraftingNodePresets;

		ECraftingResult LoadObjectNodeSet(UWorld *world);
		ECraftingResult PresetToSpec(const FName &PresetID, BIM::FModumateAssemblyPropertySpec &OutPropertySpec) const;

		ECraftingResult FetchAllPresetsForObjectType(EObjectType ObjectType, TSet<FName> &OutPresets) const;

		bool TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FName &PresetID, const FModumateObjectAssembly *&OutAssembly) const;
		bool TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FModumateObjectAssembly &OutAssembly) const;
		
		ECraftingResult UpdateProjectAssembly(const FModumateObjectAssembly &Assembly);
		ECraftingResult RemoveProjectAssemblyForPreset(const FName &PresetID);
		ECraftingResult MakeNewOrUpdateExistingPresetFromParameterSet(UWorld *World,const FModumateFunctionParameterSet &ParameterSet, FName &OutKey);

		ECraftingResult GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FModumateObjectAssembly> &OutAssemblies) const;

		ECraftingResult AddOrUpdateGraph2DRecord(FName Key, const FGraph2DRecord &Graph, FName &OutKey);
		ECraftingResult RemoveGraph2DRecord(const FName &Key);
		ECraftingResult GetGraph2DRecord(const FName &Key, FGraph2DRecord &OutGraph) const;

		// Intended as a general way to generate keys: given a base name, append with increment integers until an unused name is found
		// This pattern is used through out the app, particularly in FModumateDocument, to generate IDs for new objects
		// All such schemes should switch to this one key store
		// TODO: the key store is likely to grow, so we may need to add some scope discrimination or a more complex scheme down the road
		FName GetAvailableKey(const FName &BaseKey);

		ECraftingResult FromDocumentRecord(UWorld *World, const FModumateDocumentHeader &DocumentHeader, const FMOIDocumentRecord &DocumentRecord);
		ECraftingResult ToDocumentRecord(FMOIDocumentRecord &OutRecord) const;

		// DDL 1.0 - all to be deprecated
		enum Result
		{
			NoError = 0,
			Error,
			Found,
			NotFound
		};

		Result InitAssemblyDBs();
					
		const FModumateObjectAssembly *GetAssemblyByKey(EToolMode Mode, const FName &Key) const;
		const FModumateObjectAssembly *GetAssemblyByKey(const FName &Key) const;
	};
}