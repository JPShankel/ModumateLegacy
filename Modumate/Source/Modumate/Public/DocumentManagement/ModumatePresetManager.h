// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "ModumateCore/ModumateDecisionTree.h"
#include "Database/ModumateCrafting.h"
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

		TArray<FCraftingPreset> CraftingPresetArray, CraftingBuiltins;
		TMap<EToolMode, Modumate::FCraftingDecisionTree> CraftingDecisionTrees;

		static FString GetDefaultCraftingBuiltinPresetFileName();
		static FString GetDefaultCraftingBuiltinPresetFilePath();
		static FString GetDefaultCraftingBuiltinPresetDirectory();

		// Initialization
		Result LoadDecisionTreeForToolMode(UWorld *world,EToolMode toolMode);
		Result LoadAllDecisionTrees(UWorld *world);

		Result InitAssemblyDBs();
		Result LoadCraftingBuiltins(const FString &path);

		Result FillInMissingPresets(UWorld *world,EToolMode toolMode, FCraftingDecisionTree &craftingTree);
		Result FindOrCreatePresets(UWorld *world,EToolMode toolMode, FCraftingDecisionTree &craftingTree);

		// Exports current presets to the builtin file
		Result SaveBuiltins(UWorld *world, const FString &path);

		// Get an initialized tree
		Result MakeNewTreeForToolMode(UWorld *world,EToolMode toolMode, FCraftingDecisionTree &outTree);
		Result MakeNewTreeForAssembly(UWorld *world, EToolMode toolMode, const FName &assemblyKey, FCraftingDecisionTree &outTree);

		// Access builtins and presets
		Result GetBuiltinsForForm(EToolMode mode, const FCraftingDecisionTree &craftingTree, const FName &formGUID, TArray<FCraftingItem> &outPresets) const;
		Result GetBuiltinsForForm(const FName &subcategoryName, const FName &formName, TArray<FCraftingPreset> &presets) const;
		
		Result GetPresetsForForm(const FName &subcategoryName, const FName &formName, TArray<FCraftingPreset> &presets) const;
		Result GetPresetsForForm(EToolMode mode, const FCraftingDecisionTree &craftingTree, const FName &formGUID, TArray<FCraftingItem> &outPresets) const;

		const FCraftingPreset *GetPresetByKey(const FName &key) const;
		const FCraftingPreset *GetBuiltinByKey(const FName &key) const;
		bool IsBuiltin(const FName &key) const;
		const FCraftingPreset *FindMatchingPreset(const FName &subcategory, const FName &form, const BIM::FBIMPropertySheet &properties, const TArray<FName> &childPresets) const;

		// Preset creation/updating/removal		
		// Note: mutator commands (which must go through the game instance's command system) come in "Dispatch/Handle" pairs
		Result DispatchCreateOrUpdatePresetCommand(UWorld *world,EToolMode mode, const BIM::FBIMPropertySheet &properties, const TArray<FName> &childPresets, const FName &presetKey, FName &outPreset);
		Result HandleCreatePresetCommand(EToolMode mode, const BIM::FBIMPropertySheet &properties, const TArray<FName> &childPresets, FName &outPreset);
		Result HandleUpdatePresetCommand(EToolMode mode, const BIM::FBIMPropertySheet &properties, const TArray<FName> &childPresets, const FName &presetKey);

		Result DispatchRemovePresetCommand(UWorld *world, EToolMode toolMode, const FName &presetKey, const FName &replacementKey);
		Result HandleRemovePresetCommand(UWorld *world,EToolMode mode,const FName &presetKey, const FName &replacementKey);

		// Not a mutator, produces a clean preset without adding it to the db
		Result MakeNewPresetFromCraftingTreeAndFormGUID(EToolMode mode, Modumate::FCraftingDecisionTree &craftingTree, const FName &formGUID, FCraftingPreset &outPreset) const;
		
		// Data analysis
		Result TryGetPresetPropertiesRecurse(const FName &presetKey, BIM::FBIMPropertySheet &props) const;
		static Result GatherCraftingProperties(const Modumate::FCraftingDecisionTree &craftingTree, Modumate::BIM::FModumateAssemblyPropertySpec &propertySheet, bool usePresets);
		Result ForEachDependentPreset(const FName &presetKey, std::function<void(const FName &name)> fn) const;
		Result GetDependentPresets(const FName &presetKey, TArray<FName> &dependents) const;
		Result ForEachChildPreset(const FName &presetKey, std::function<void(const FName &name)> fn) const;
		Result GetChildPresets(const FName &presetKey, TArray<FName> &children) const;
		Result GatherNodePresetProperties(const FCraftingDecisionTreeNode &formNode, BIM::FBIMPropertySheet &properties, TArray<FName> &childPresets) const;

		// Tree manipulation
		Result UpdateCraftingNodeWithPreset(FCraftingDecisionTreeNode &node, const FName &presetKey) const;
		Result UpdateCraftingTreeWithPreset(FCraftingDecisionTree &craftingTree, const FName &formGUID, const FName &presetKey) const;
		Result UpdateCraftingTreeWithBuiltin(UWorld *world,EToolMode mode, FCraftingDecisionTree &craftingTree, const FName &formGUID, const FName &presetKey);
			
		static FCraftingDecisionTreeNode *GetFormInActiveLayerByGUID(FCraftingDecisionTree &craftingTree, const FName &formGUID);
		static FCraftingDecisionTreeNode *GetChildNodeByPreset(FCraftingDecisionTreeNode &craftingTree, const FCraftingPreset &preset);

		// Assemblies
		Result BuildAssemblyFromCraftingTree(UWorld *world, EToolMode toolMode, const FCraftingDecisionTree &craftingTree, FModumateObjectAssembly &outAssembly, bool usesPresets, const int32 showOnlyLayerID = -1) const;
		Result GetAssembyKeysForPreset(EToolMode mode, const FName &presetKey, TArray<FName> &assemblyKeys) const;
		Result RefreshExistingAssembly(UWorld *world, EToolMode mode, const FName &assemblyKey);
		Result ReplacePresetInAssembly(UWorld *world, EToolMode mode, const FName &assemblyKey, const FName &oldPreset, const FName &newPreset);
		
		const FModumateObjectAssembly *GetAssemblyByKey(EToolMode Mode, const FName &Key) const;
		const FModumateObjectAssembly *GetAssemblyByKey(const FName &Key) const;
	};
}