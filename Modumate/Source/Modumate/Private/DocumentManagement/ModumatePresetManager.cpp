// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumatePresetManager.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Database/ModumateObjectEnums.h"
#include "Database/ModumateCraftingTreeBuilder.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Algo/Transform.h"

using namespace Modumate;

void FCraftingPreset::UpdatePresetNameFromProperties()
{
	if (PresetName.IsEmpty() && !Properties.TryGetProperty(BIM::EScope::Preset, BIM::Parameters::Name, PresetName))
	{
		BIM::FNameType altNameProps[] = { BIM::Parameters::Name,BIM::Parameters::DimensionSetName,BIM::Parameters::StyleName };
		for (auto &np : altNameProps)
		{
			Properties.ForEachProperty([this,np](const FString &Param, const FModumateCommandParameter &MCP)
			{
				BIM::FValueSpec vs(*Param);
				if (vs.Name == np)
				{
					PresetName = MCP.AsString();
				}
			});
		}
	}
}

void FCraftingPreset::UpdateArchiveFromProperties()
{
	PropertyArchive.Empty();
	Properties.ForEachProperty([this](const FString &name, const Modumate::FModumateCommandParameter &prop)
	{
		PropertyArchive.Add(name, prop.AsJSON());
	});
}

void FCraftingPreset::UpdatePropertiesFromArchive()
{
	for (auto &kvp : PropertyArchive)
	{
		Modumate::FModumateCommandParameter prop;
		prop.FromJSON(kvp.Value);
		Properties.SetValue(kvp.Key, prop);
	}
}

namespace Modumate
{
	FPresetManager::FPresetManager()
	{}

	FPresetManager::~FPresetManager()
	{}

	ECraftingResult FPresetManager::FromDocumentRecord(UWorld *World, const FModumateDocumentHeader &DocumentHeader, const FMOIDocumentRecord &DocumentRecord)
	{
		AEditModelGameMode_CPP *mode = Cast<AEditModelGameMode_CPP>(World->GetAuthGameMode());
		AssembliesByObjectType.Empty();

		if (DocumentHeader.Version < MinimumReadableVersion)
		{
			mode->ObjectDatabase->InitPresetManagerForNewDocument(*this);
			return ECraftingResult::Success;
		}

		// TODO: deprecate when we no longer serialize assemblies
		for (auto &ca : DocumentRecord.CustomAssemblies)
		{
			FModumateObjectAssembly newAsm;
			FModumateObjectAssembly::FromDataRecord_DEPRECATED(ca, *mode->ObjectDatabase, *this, newAsm);
			if (!UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(newAsm.ObjectType))
			{
				AssemblyDBs_DEPRECATED.Find(ca.ToolMode)->AddData(newAsm);
			}
			else
			{
				// custom assemblies for DDL2 are moved to the new database
				// they will not have root presets, so they will be set to the default assembly for their type if edited
				FAssemblyDataCollection &db = AssembliesByObjectType.FindOrAdd(newAsm.ObjectType);
				db.AddData(newAsm);
			}
		}

		KeyStore = DocumentRecord.KeyStore;
		CraftingNodePresets.FromDataRecords(DocumentRecord.CraftingPresetArrayV2);

		BIM::FCraftingPresetCollection &presetCollection = CraftingNodePresets;

		// In the DDL2verse, assemblies are stored solely by their root preset ID then rebaked into runtime assemblies on load
		for (auto &presetID : DocumentRecord.ProjectAssemblyPresets)
		{
			// this ensure will fire if expected presets have become obsolete, resave to fix
			if (!ensureAlways(CraftingNodePresets.Presets.Contains(presetID)))
			{
				continue;
			}

			EObjectType objectType = CraftingNodePresets.GetPresetObjectType(presetID);
			if (ensureAlways(UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(objectType)))
			{
				BIM::FModumateAssemblyPropertySpec spec;
				PresetToSpec(presetID, spec);
				FModumateObjectAssembly outAssembly;
				ECraftingResult result = UModumateObjectAssemblyStatics::DoMakeAssembly(*mode->ObjectDatabase, *this, spec, outAssembly);
				if (ensureAlways(result == ECraftingResult::Success))
				{
					outAssembly.DatabaseKey = presetID;
					UpdateProjectAssembly(outAssembly);
				}
				else
				{
					return result;
				}
			}
			else
			{
				return ECraftingResult::Error;
			}
		}

		GraphCollection = DocumentRecord.CustomGraph2DRecords;

		return ECraftingResult::Success;
	}

	ECraftingResult FPresetManager::ToDocumentRecord(FMOIDocumentRecord &OutRecord) const
	{
		CraftingNodePresets.ToDataRecords(OutRecord.CraftingPresetArrayV2);
		OutRecord.KeyStore = KeyStore;

		for (auto &db : AssemblyDBs_DEPRECATED)
		{
			TArray<FCustomAssemblyRecord> asmRecs = db.Value.GetDataRecords<FCustomAssemblyRecord>();
			for (auto &a : asmRecs) 
			{ 
				a.ToolMode = db.Key; 
			}
			OutRecord.CustomAssemblies.Append(asmRecs);
		}

		for (auto &db : AssembliesByObjectType)
		{
			for (auto &preset : db.Value.DataMap)
			{
				if (ensureAlways(CraftingNodePresets.Presets.Contains(preset.Key)))
				{
					OutRecord.ProjectAssemblyPresets.Add(preset.Key);
				}
			}
		}

		OutRecord.CustomGraph2DRecords = GraphCollection;

		return ECraftingResult::Success;
	}

	FName FPresetManager::GetAvailableKey(const FName &BaseKey)
	{
		FName newKey;
		int32 num = 0;
		do 
		{
			newKey = *FString::Printf(TEXT("%s-%d"), *BaseKey.ToString(), ++num);
		} while (KeyStore.Contains(newKey));

		KeyStore.Add(newKey);
		return newKey;
	}

	ECraftingResult FPresetManager::GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FModumateObjectAssembly> &OutAssemblies) const
	{
		const FAssemblyDataCollection *db = AssembliesByObjectType.Find(ObjectType);
		if (db == nullptr)
		{
			return ECraftingResult::Success;
		}
		for (auto &kvp : db->DataMap)
		{
			OutAssemblies.Add(kvp.Value);
		}
		return ECraftingResult::Success;
	}

	ECraftingResult FPresetManager::AddOrUpdateGraph2DRecord(FName Key, const FGraph2DRecord &Graph, FName &OutKey)
	{
		if (Key.IsNone())
		{
			static const FName defaultGraphBaseKey(TEXT("Graph2D"));
			Key = GetAvailableKey(defaultGraphBaseKey);
		}

		OutKey = Key;

		GraphCollection.Emplace(Key, Graph);
		return ECraftingResult::Success;
	}

	ECraftingResult FPresetManager::RemoveGraph2DRecord(const FName &Key)
	{
		int32 numRemoved = GraphCollection.Remove(Key);
		return (numRemoved > 0) ? ECraftingResult::Success : ECraftingResult::Error;
	}

	ECraftingResult FPresetManager::GetGraph2DRecord(const FName &Key, FGraph2DRecord &OutGraph) const
	{
		if (GraphCollection.Contains(Key))
		{
			OutGraph = GraphCollection.FindChecked(Key);
			return ECraftingResult::Success;
		}

		return ECraftingResult::Error;
	}

	ECraftingResult FPresetManager::RemoveProjectAssemblyForPreset(const FName &PresetID)
	{
		const FModumateObjectAssembly *assembly;
		EObjectType objectType = CraftingNodePresets.GetPresetObjectType(PresetID);
		if (TryGetProjectAssemblyForPreset(objectType, PresetID, assembly) && ensureAlways(assembly != nullptr))
		{
			FAssemblyDataCollection *db = AssembliesByObjectType.Find(objectType);
			if (ensureAlways(db != nullptr))
			{
				db->RemoveData(*assembly);
				return ECraftingResult::Success;
			}
		}
		return ECraftingResult::Error;
	}

	ECraftingResult FPresetManager::UpdateProjectAssembly(const FModumateObjectAssembly &Assembly)
	{
		if (!ensureAlways(UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(Assembly.ObjectType)))
		{
			return ECraftingResult::Error;
		}
		FAssemblyDataCollection &db = AssembliesByObjectType.FindOrAdd(Assembly.ObjectType);
		db.AddData(Assembly);
		return ECraftingResult::Success;
	}

	static void recurseSetNodeProperties(
		FCraftingDecisionTreeNode &node,
		const BIM::FBIMPropertySheet &props,
		std::function<bool(const FCraftingDecisionTreeNode &)> shouldContinue)
	{
		// Note: terminal nodes are constant values used by 'Select' decisions that shouldn't be altered
		if (node.Data.Type != EDecisionType::Terminal)
		{
			if (node.Data.Type == EDecisionType::String || node.Data.Type == EDecisionType::Dimension)
			{
				node.Data.Value.Empty();
			}
			props.ForEachProperty([&node](const FString &propertyName, const FModumateCommandParameter &prop)
			{
				BIM::FValueSpec val(*prop.ParameterName);
				if (node.Data.Properties.HasProperty(val.Scope, val.Name))
				{
					node.Data.Properties.SetProperty(val.Scope, val.Name, prop);
				}
				if (node.Data.PropertyValueBindings.Contains(propertyName))
				{
					node.Data.Properties.SetProperty(val.Scope, val.Name, prop);
					node.Data.Value = prop;
				}
			});
		}

		auto getSubfieldPropertyMatches = [props](const TDecisionTreeNode<FCraftingItem> &subfield)
		{
			int32 count = 0;

			TArray<FString> keys;
			subfield.Data.Properties.GetValueNames(keys);

			for (const auto &k : keys)
			{
				FModumateCommandParameter prop;
				BIM::FValueSpec vs(*k);
				if (props.TryGetProperty(vs.Scope, vs.Name, prop))
				{
					if (prop.AsJSON() == subfield.Data.Properties.GetProperty(vs.Scope, vs.Name).AsJSON())
					{
						++count;
					}
				}
			}
			return count;
		};

		if (node.Data.Type == EDecisionType::Select)
		{
			int maxCount = 0;
			int32 maxIndex = 0;
			for (int32 i = 0; i < node.Subfields.Num(); ++i)
			{
				if (node.Subfields[i].Data.Type != EDecisionType::Terminal)
				{
					if (node.Subfields[i].Data.GUID == node.Data.SelectedSubnodeGUID)
					{
						if (shouldContinue(node.Subfields[i]))
						{
							recurseSetNodeProperties(node.Subfields[i], props, shouldContinue);
						}
						maxIndex = i;
					}
				}
				else
				{
					int32 count = getSubfieldPropertyMatches(node.Subfields[i]);
					if (count > maxCount)
					{
						maxCount = count;
						maxIndex = i;
					}
				}
			}
			node.SetSelectedSubfield(maxIndex);
		}
		else
		{
			for (auto &sf : node.Subfields)
			{
				if (sf.Data.Type != EDecisionType::Terminal && shouldContinue(sf))
				{
					recurseSetNodeProperties(sf, props, shouldContinue);
				}
			}
		}
	}

	// We recurse the entire tree when assimilating an existing assembly
	static void recurseEntireTree(FCraftingDecisionTreeNode &node,const BIM::FBIMPropertySheet &props)
	{
		recurseSetNodeProperties(node, props, [](const FCraftingDecisionTreeNode &node) {return true; });
	}

	// We halt at a child preset form when we're filling in a parent's preset values
	static void recursePresetForm(FCraftingDecisionTreeNode &node,const BIM::FBIMPropertySheet &props)
	{
		recurseSetNodeProperties(node, props, [](const FCraftingDecisionTreeNode &node) {return !node.Data.HasPreset; });
	}

	/*
	Initializers
	*/
	FString FPresetManager::GetDefaultCraftingBuiltinPresetFileName()
	{
		return TEXT("craftingBuiltins.mdps");
	}

	FString FPresetManager::GetDefaultCraftingBuiltinPresetFilePath()
	{
		return FPaths::Combine(GetDefaultCraftingBuiltinPresetDirectory(), GetDefaultCraftingBuiltinPresetFileName());
	}

	FString FPresetManager::GetDefaultCraftingBuiltinPresetDirectory()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("NonUAssets"));
	}

	ECraftingResult FPresetManager::ReadBIMTable(UDataTable *DataTable, BIM::FCraftingPresetCollection &Target)
	{
		if (!ensureAlways(DataTable != nullptr))
		{
			return ECraftingResult::Error;
		}

		TArray<FString> errors;
		FString tableName;
		DataTable->GetName(tableName);

		if (Target.ReadDataTable(DataTable, errors) == ECraftingResult::Success)
		{
			if (errors.Num() > 0)
			{
				for (auto &msg : errors)
				{
					UE_LOG(LogTemp, Display, TEXT(" - OBJECT TABLE ERROR %s"), *msg);
				}
				ensureAlwaysMsgf(false, TEXT("COMPILE ERRORS IN NODE TABLE %s"),*tableName);
				return ECraftingResult::Error;
			}
			return ECraftingResult::Success;
		}
		return ECraftingResult::Error;
	}

	ECraftingResult FPresetManager::LoadObjectNodeSet(UWorld *world)
	{
		TArray<FString> errors;
		CraftingNodePresets = BIM::FCraftingPresetCollection();
		/*
		TODO: eventually this wants to be compiled into a binary format so we're not parsing scripts, but during development we still parse on load
		We want a good deal of error checking here, with ensures, because this is how the designers debug their code
		*/

		UObjectLibrary *bimTables = FindObject<UObjectLibrary>(ANY_PACKAGE, TEXT("/Game/Tables/BIM/BIMTables.BIMTables"));

		if (!ensureAlwaysMsgf(bimTables != nullptr, TEXT("Could not find BIM table library")))
		{
			return ECraftingResult::Error;
		}

		TArray<UDataTable*> objects;
		bimTables->GetObjects(objects);

		for (auto &dataTable : objects)
		{
			ECraftingResult result = ReadBIMTable(dataTable, CraftingNodePresets);
			if (result != ECraftingResult::Success)
			{
				return result;
			}
		}

		if (CraftingNodePresets.ParseScriptFile(FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMNodeGraph.mbn"), errors) == ECraftingResult::Success)
		{
			if (errors.Num() > 0)
			{
				for (auto &msg : errors)
				{
					UE_LOG(LogTemp, Display, TEXT(" - OBJECT NODE ERROR %s"), *msg);
				}
				ensureAlwaysMsgf(false, TEXT("COMPILE ERRORS IN OBJECT NODE SCRIPT"));
				return ECraftingResult::Error;
			}

			// TODO: extend syntax to mark assembly presets for import, for now get well known list
			TArray<FName> *defaultAssemblies = CraftingNodePresets.Lists.Find(TEXT("DefaultAssemblies"));
			AEditModelGameMode_CPP *mode = Cast<AEditModelGameMode_CPP>(world->GetAuthGameMode());
			if (defaultAssemblies != nullptr)
			{
				for (auto &presetID : *defaultAssemblies)
				{
					BIM::FModumateAssemblyPropertySpec spec;
					PresetToSpec(presetID, spec);

					if (ensureAlways(spec.ObjectType != EObjectType::OTNone))
					{ 
						FModumateObjectAssembly outAssembly;

						UModumateObjectAssemblyStatics::DoMakeAssembly(*mode->ObjectDatabase, *this, spec, outAssembly);
						outAssembly.DatabaseKey = presetID;

						ECraftingResult result = UpdateProjectAssembly(outAssembly);
						if (result != ECraftingResult::Success)
						{
							UE_LOG(LogTemp, Display, TEXT("FAILED TO MAKE ASSEMBLY FOR PRESET %s"), *presetID.ToString());
							ensureAlwaysMsgf(false, TEXT("FAILED TO MAKE ASSEMBLY FOR PRESET %s"), *presetID.ToString());
							return result;
						}
					}
					else
					{
						UE_LOG(LogTemp, Display, TEXT("NO OBJECT TYPE FOR PRESET %s"),*presetID.ToString());
						ensureAlwaysMsgf(false, TEXT("NO OBJECT TYPE FOR PRESET %s"), *presetID.ToString());
						return ECraftingResult::Error;
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("CRAFTING NODE SCRIPT PARSE FAILURE"));
			ensureAlwaysMsgf(false, TEXT("CRAFTING NODE SCRIPT PARSE FAILURE"));
			return ECraftingResult::Error;
		}

		DraftingNodePresets = BIM::FCraftingPresetCollection();
		if (DraftingNodePresets.ParseScriptFile(FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("DrawingSetNodeGraph.mbn"), errors) == ECraftingResult::Success)
		{
			if (errors.Num() > 0)
			{
				for (auto &msg : errors)
				{
					UE_LOG(LogTemp, Display, TEXT(" - DRAWING SET NODE ERROR %s"), *msg);
				}
				UE_LOG(LogTemp, Display, TEXT("COMPILE ERRORS IN DRAWING SET NODE SCRIPT"));
				ensureAlwaysMsgf(false, TEXT("COMPILE ERRORS IN DRAWING SET NODE SCRIPT"));
				return ECraftingResult::Error;
			}
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("DRAWING SET NODE SCRIPT PARSE FAILURE"));
			ensureAlwaysMsgf(false, TEXT("DRAWING SET NODE SCRIPT PARSE FAILURE"));
		}

		return ECraftingResult::Success;
	}

	bool FPresetManager::TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FName &PresetID, const FModumateObjectAssembly *&OutAssembly) const
	{
		const DataCollection<FModumateObjectAssembly> *db = AssembliesByObjectType.Find(ObjectType);

		// It's legal for an object database to not exist
		if (db == nullptr)
		{
			OutAssembly = nullptr;
			return false;
		}

		OutAssembly = db->GetData(PresetID);
		return (OutAssembly != nullptr);
	}


	FPresetManager::Result FPresetManager::LoadDecisionTreeForToolMode(UWorld *world, EToolMode toolMode)
	{

		CraftingDecisionTrees.Remove(toolMode);
		FCraftingDecisionTree &craftingTree = CraftingDecisionTrees.Add(toolMode);

		AEditModelGameMode_CPP *mode = Cast<AEditModelGameMode_CPP>(world->GetAuthGameMode());

		FString formName;
		switch (toolMode)
		{
			case EToolMode::VE_WALL: formName = TEXT("WALL_ROOT"); break;
			case EToolMode::VE_FLOOR: formName = TEXT("FLOOR_ROOT"); break;
			case EToolMode::VE_ROOF_FACE: formName = TEXT("ROOF_ROOT"); break;
			case EToolMode::VE_FINISH: formName = TEXT("FINISH_ROOT"); break;
			case EToolMode::VE_TRIM: formName = TEXT("TRIM_ROOT"); break;
			case EToolMode::VE_WINDOW: formName = TEXT("WINDOW_ROOT"); break;
			case EToolMode::VE_DOOR: formName = TEXT("DOOR_ROOT"); break;
			case EToolMode::VE_CABINET: formName = TEXT("CABINET_ROOT"); break;
			case EToolMode::VE_COUNTERTOP: formName = TEXT("COUNTERTOP_ROOT"); break;

			// DDL 2.0 data types do not have DDL 1.0 decision trees
			case EToolMode::VE_STRUCTURELINE: ensureAlways(false); return Result::Error;
		};

		if (formName.IsEmpty())
		{
			ensureAlways(false);
			return Error;
		}

		FCraftingTreeBuilder treeBuilder;
		TArray<FString> compileErrors;
		if (treeBuilder.ParseDDL(FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("CraftingTrees.mdl"), compileErrors))
		{
			TArray<FString> buildErrors;
			craftingTree.RootNode.Data.SubCategory = TEXT("NoSubcategory");
			if (!treeBuilder.BuildDecisionTree(*mode->ObjectDatabase, formName, craftingTree, buildErrors))
			{
				for (auto &msg : buildErrors)
				{
					UE_LOG(LogTemp, Display, TEXT(" - TEST WALL TREE ERROR %s"), *msg);
				}
				ensureAlwaysMsgf(false, TEXT("BUILD ERRORS IN TEST WALL SCRIPT"));
			}
		}
		else
		{
			for (auto &msg : compileErrors)
			{
				UE_LOG(LogTemp, Display, TEXT(" - TEST WALL TREE ERROR %s"), *msg);
			}
			ensureAlwaysMsgf(false, TEXT("COMPILE ERRORS IN TEST WALL SCRIPT"));
		}

		/*
		Infuse dynamic list types with default values
		*/
		for (auto &instanceType : craftingTree.DynamicListElementTypes)
		{
			FCraftingDecisionTreeNode &instance = instanceType.Value;
			TArray<FCraftingDecisionTreeNode*> presetNodes;

			/*
			Gather all nodes in this list that have presets...each node will have a form key and a subcategory
			*/
			instance.WalkDepthFirst([&presetNodes](FCraftingDecisionTreeNode &node) {
				if (node.Data.HasPreset)
				{
					presetNodes.Add(&node);
				}
				return true;
			});

			/*
			For each discovered node, get either the first user preset or (in the absence of any) the first builtin and set the preset form accordingly
			*/
			for (auto &formNode : presetNodes)
			{
				TArray<FCraftingPreset> presets;
				GetPresetsForForm(formNode->Data.SubCategory, formNode->Data.PresetFormKey, presets);
				if (presets.Num() == 0)
				{
					GetBuiltinsForForm(formNode->Data.SubCategory, formNode->Data.PresetFormKey, presets);
				}
				if (presets.Num() > 0)
				{
					recursePresetForm(*formNode, presets[0].Properties);
				}
			}
		}
		return NoError;
	}

#define PSMLOADDECISIONTREE(x) if (LoadDecisionTreeForToolMode(world,(x))!=NoError){ret=Error;}
	FPresetManager::Result FPresetManager::LoadAllDecisionTrees(UWorld *world)
	{
		FPresetManager::Result ret = NoError;
		PSMLOADDECISIONTREE(EToolMode::VE_WALL);
		PSMLOADDECISIONTREE(EToolMode::VE_FLOOR);
		PSMLOADDECISIONTREE(EToolMode::VE_ROOF_FACE);
		PSMLOADDECISIONTREE(EToolMode::VE_FINISH);
		PSMLOADDECISIONTREE(EToolMode::VE_TRIM);
		PSMLOADDECISIONTREE(EToolMode::VE_WINDOW);
		PSMLOADDECISIONTREE(EToolMode::VE_DOOR);
		PSMLOADDECISIONTREE(EToolMode::VE_CABINET);
		PSMLOADDECISIONTREE(EToolMode::VE_COUNTERTOP);

		return ret;
	}

	FPresetManager::Result FPresetManager::InitAssemblyDBs()
	{
		AssemblyDBs_DEPRECATED.Reset();
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_WALL, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_FLOOR, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_DOOR, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_WINDOW, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_STAIR, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_RAIL, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_CABINET, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_COUNTERTOP, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_FINISH, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_PLACEOBJECT, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_TRIM, DataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_ROOF_FACE, DataCollection<FModumateObjectAssembly>());

		AssembliesByObjectType.Add(EObjectType::OTStructureLine, DataCollection<FModumateObjectAssembly>());
		AssembliesByObjectType.Add(EObjectType::OTRoofPerimeter, DataCollection<FModumateObjectAssembly>());

		LoadCraftingBuiltins(GetDefaultCraftingBuiltinPresetFilePath());
		return NoError;
	}

	FPresetManager::Result FPresetManager::LoadCraftingBuiltins(const FString &path)
	{
		FString FileJsonString;
		bool bLoadFileSuccess = FFileHelper::LoadFileToString(FileJsonString, *path);
		if (!bLoadFileSuccess || FileJsonString.IsEmpty())
		{
			return Error;
		}

		auto JsonReader = TJsonReaderFactory<>::Create(FileJsonString);

		TSharedPtr<FJsonObject> FileJson;
		bool bLoadJsonSuccess = FJsonSerializer::Deserialize(JsonReader, FileJson) && FileJson.IsValid();
		if (!bLoadJsonSuccess)
		{
			return Error;
		}

		FModumatePresetRecord_DEPRECATED presetRec;
		if (FJsonObjectConverter::JsonObjectToUStruct<FModumatePresetRecord_DEPRECATED>(FileJson.ToSharedRef(), &presetRec))
		{
			if (presetRec.CraftingPresetArray.Num() > 0)
			{
				CraftingBuiltins = presetRec.CraftingPresetArray;
			}
			for (auto &preset : CraftingBuiltins)
			{
				preset.UpdatePropertiesFromArchive();
				preset.UpdatePresetNameFromProperties();
			}
			return NoError;
		}

		return Error;
	}

	// Makes sure that every presettable node in the tree has a preset, either by creating one or choosing the first one
	// TODO: final design is to require a pre-existing assembly to begin editing and so there will never be missing presets except in development
	FPresetManager::Result FPresetManager::FillInMissingPresets(UWorld *world,EToolMode toolMode, Modumate::FCraftingDecisionTree &craftingTree)
	{
		int32 dlIndex = craftingTree.RootNode.Data.DynamicListElementIndex;
		FCraftingDecisionTree::FDecisionNodeArray *instances = craftingTree.DynamicListElementInstances.Find(craftingTree.RootNode.Data.DynamicList);

		if (ensureAlways(instances != nullptr && dlIndex < instances->Num()))
		{
			FCraftingDecisionTreeNode &instanceNode = (*instances)[dlIndex];

			// Open nodes form the visible elements in the crafting dialog
			TArray<FCraftingItem> openNodes = instanceNode.GetOpenNodes();

			//reverse iteration because lower-level presets should be established before higher ones
			for (int32 i = openNodes.Num() - 1; i >= 0; --i)
			{
				FCraftingItem &openNode = openNodes[i];
				if (openNode.HasPreset && openNode.SelectedPreset.IsNone())
				{
					FCraftingDecisionTreeNode *node = instanceNode.FindNodeByGUID(openNode.GUID);

					TArray<FCraftingPreset> presets;
					GetPresetsForForm(openNode.SubCategory, openNode.PresetFormKey, presets);

					// If we don't have any presets for this form, import the first builtin
					if (presets.Num() == 0)
					{
						TArray<FCraftingPreset> builtins;
						GetBuiltinsForForm(openNode.SubCategory, openNode.PresetFormKey, builtins);

						if (ensureAlways(node != nullptr && builtins.Num() > 0))
						{
							// Fill the properties of the builtin into the node and its children
							recurseEntireTree(*node, builtins[0].Properties);

							// Gather the properties and child presets for the node (child presets should be empty for a bultin)
							BIM::FBIMPropertySheet newProperties;
							TArray<FName> newChildPresets;
							GatherNodePresetProperties(*node, newProperties, newChildPresets);

							// Establish a new preset for these settings and set the new preset key into the existing node
							FName newPresetKey;
							DispatchCreateOrUpdatePresetCommand(world,toolMode, newProperties, newChildPresets,NAME_None,newPresetKey);
							UpdateCraftingNodeWithPreset(*node, newPresetKey);
						}
						else
						{
							return Error;
						}
					}
					// If we do have a preset for this form, apply the first one
					else
					{
						UpdateCraftingNodeWithPreset(*node, presets[0].Key);
					}
				}
			}
		}
		else
		{
			return Error;
		}
		return NoError;
	}

	// For each node of the tree, find or a create a preset corresponding to its current state (part of importing a builtin)
	FPresetManager::Result FPresetManager::FindOrCreatePresets(UWorld *world,EToolMode toolMode, FCraftingDecisionTree &craftingTree)
	{
		int32 dlIndex = craftingTree.RootNode.Data.DynamicListElementIndex;
		FCraftingDecisionTree::FDecisionNodeArray *instances = craftingTree.DynamicListElementInstances.Find(craftingTree.RootNode.Data.DynamicList);

		if (ensureAlways(instances != nullptr && dlIndex < instances->Num()))
		{
			FCraftingDecisionTreeNode &instanceNode = (*instances)[dlIndex];

			TArray<FCraftingItem> openNodes = instanceNode.GetOpenNodes();
			//reverse iteration because lower-level presets should be established before higher ones
			for (int32 i = openNodes.Num() - 1; i >= 0; --i)
			{
				FCraftingItem &openNode = openNodes[i];

				if (openNode.HasPreset)
				{
					FCraftingDecisionTreeNode *node = instanceNode.FindNodeByGUID(openNode.GUID);
					if (ensureAlways(node != nullptr))
					{
						// Gather the node's properties and try to find a preset that matches them
						BIM::FBIMPropertySheet properties;
						TArray<FName> childPresets;
						GatherNodePresetProperties(*node, properties, childPresets);

						const FCraftingPreset *preset = FindMatchingPreset(node->Data.SubCategory, node->Data.PresetFormKey, properties, childPresets);

						// If no preset matches current settings, create one
						if (preset == nullptr)
						{
							FName newPresetKey;
							if (ensureAlways(DispatchCreateOrUpdatePresetCommand(world,toolMode, properties, childPresets, NAME_None,newPresetKey) == FPresetManager::NoError))
							{
								preset = GetPresetByKey(newPresetKey);
							}
							else
							{
								return Error;
							}
						}

						if (ensureAlways(preset != nullptr))
						{
							UpdateCraftingNodeWithPreset(*node, preset->Key);
						}
						else
						{
							return Error;
						}
					}
					else
					{
						return Error;
					}
				}
			}
		}
		else
		{
			return Error;
		}
		return NoError;
	}

	// Export presets to builtins.  Not a user-facing feature...used to develop builtins
	FPresetManager::Result FPresetManager::SaveBuiltins(UWorld *world, const FString &path)
	{
		FModumatePresetRecord_DEPRECATED rec;

		if (CraftingPresetArray.Num() == 0)
		{
			PlatformFunctions::ShowMessageBox(TEXT("There are no presets to export."), TEXT("Builtins"), PlatformFunctions::Okay);
			return Error;
		}

		int32 builtIn = 0;
		for (auto &preset : CraftingPresetArray)
		{
			FCraftingPreset newPreset = preset;

			newPreset.Key = *newPreset.Key.ToString().Replace(TEXT("PRESET"), TEXT("BUILTIN"));

			TArray<FName> childStack = preset.ChildPresets;
			newPreset.ChildPresets.Empty();

			while (childStack.Num() > 0)
			{
				FName psKey = childStack.Pop();
				const FCraftingPreset *childPreset = GetPresetByKey(psKey);
				if (ensureAlways(childPreset != nullptr))
				{
					childPreset->Properties.ForEachProperty([&newPreset](const FString &name, const FModumateCommandParameter &mcp)
					{
						BIM::FValueSpec vs(*name);
						newPreset.Properties.SetProperty(vs.Scope, vs.Name, mcp);
					});
					for (auto &cp : childPreset->ChildPresets)
					{
						childStack.Push(cp);
					}
				}
			}
			newPreset.UpdateArchiveFromProperties();
			rec.CraftingPresetArray.Add(newPreset);
		}

		CraftingBuiltins = rec.CraftingPresetArray;

		auto builtinPresetsJSON = FJsonObjectConverter::UStructToJsonObject<FModumatePresetRecord_DEPRECATED>(rec);

		FString ProjectJsonString;
		TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&ProjectJsonString);

		bool bWriteJsonSuccess = FJsonSerializer::Serialize(builtinPresetsJSON.ToSharedRef(), JsonStringWriter);
		if (!bWriteJsonSuccess)
		{
			return Error;
		}

		return FFileHelper::SaveStringToFile(ProjectJsonString, *path) ? NoError : Error;
	}

/*
Starting point: get a tree
*/

	FPresetManager::Result FPresetManager::MakeNewTreeForToolMode(UWorld *world, EToolMode toolMode, FCraftingDecisionTree &outTree)
	{
		outTree = CraftingDecisionTrees.FindOrAdd(toolMode);
		outTree.Reset();
		outTree.RootNode.Data.DynamicList = NAME_None;
		outTree.RootNode.Data.DynamicListElementIndex = 0;
		for (auto &oi : outTree.DynamicListElementInstances)
		{
			if (oi.Value.Num() > 0)
			{
				FCraftingDecisionTreeNode *dlist = outTree.FindNodeByGUID(oi.Key);
				if (dlist != nullptr)
				{
					outTree.RootNode.Data.DynamicList = oi.Key;
					outTree.RootNode.Data.DynamicListElementIndex = 0;
				}
			}
		}
		return FillInMissingPresets(world, toolMode, outTree);
	}

	FPresetManager::Result FPresetManager::MakeNewTreeForAssembly(
		UWorld *world, 
		EToolMode toolMode, 
		const FName &assemblyKey, 
		FCraftingDecisionTree &outTree)
	{
		outTree = CraftingDecisionTrees.FindOrAdd(toolMode);
		outTree.RootNode.Data.DynamicList = NAME_None;
		outTree.RootNode.Data.DynamicListElementIndex = 0;

		const FModumateObjectAssembly *existingAsm = GetAssemblyByKey(toolMode, assemblyKey);
		if (ensureAlways(existingAsm != nullptr))
		{
			BIM::FModumateAssemblyPropertySpec spec;
			existingAsm->FillSpec(spec);

			recurseEntireTree(outTree.RootNode, spec.RootProperties);

			TArray<FName> dlGUIDs;
			outTree.DynamicListElementTypes.GetKeys(dlGUIDs);

			if (ensureAlways(dlGUIDs.Num() > 0))
			{
				outTree.RootNode.Data.DynamicList = dlGUIDs[0];
				for (int32 i = 0; i < spec.LayerProperties.Num(); ++i)
				{
					BIM::FBIMPropertySheet props = spec.LayerProperties[i];
					FString presetKey;

					outTree.MakeDynamicListElementAt(dlGUIDs[0], i);
					outTree.RootNode.Data.DynamicListElementIndex = i;

					FCraftingDecisionTree::FDecisionNodeArray &arr = outTree.DynamicListElementInstances[dlGUIDs[0]];
					FCraftingDecisionTreeNode &node = arr.Last();

					FName formName, subcategoryName;

					// TODO: Select nodes in DDL assemblies have their selected subfield set below by the "walker" 
					// code that asserts a preset's selected children. Deprecated old portals do not have presets defined at
					// higher levels and so old portals must have their Select nodes set to the child corresponding to the property being set
					// DEPRECATED old portals only have two high level Selects: configuration and part set. Other values (width, height, material)
					// are subordinate to these selections and so come along once their parent-level Selects are set
					bool isPortal = false;
					if (props.TryGetProperty(BIM::EScope::Portal, BIM::Parameters::Form, formName) &&
						props.TryGetProperty(BIM::EScope::Portal, BIM::Parameters::Subcategory, subcategoryName))
					{
						isPortal = true;
						// "MaterialColor" value interferes with some color channels and should only apply to layered assemblies
						props.RemoveProperty(BIM::EScope::MaterialColor, BIM::Parameters::Color);
						props.RemoveProperty(BIM::EScope::MaterialColor, BIM::Parameters::MaterialKey);

						const BIM::FNameType params[] = { BIM::Parameters::Configuration,BIM::Parameters::PartSet };

						for (const auto &param : params)
						{
							FModumateCommandParameter selectVal;
							if (props.TryGetProperty(BIM::EScope::Portal, param, selectVal))
							{
								FCraftingDecisionTreeNode *valueNode = node.FindNode([selectVal, param](const FCraftingItem &n) {
									FModumateCommandParameter val;
									if (n.Type != EDecisionType::Select && n.Properties.TryGetProperty(BIM::EScope::Portal, param, val))
									{
										return val.Equals(selectVal);
									}
									return false;
								});

								if (ensureAlways(valueNode != nullptr))
								{
									FCraftingDecisionTreeNode *parent = node.FindNodeByGUID(valueNode->Data.ParentGUID);
									if (ensureAlways(parent != nullptr && parent->Data.Type == EDecisionType::Select))
									{
										parent->Data.SelectedSubnodeGUID = valueNode->Data.GUID;
									}
								}
							}
						}
					}
					else
					{
						props.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Form, formName);
						props.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Subcategory, subcategoryName);
					}

					if (ensureAlways(!formName.IsNone() && !subcategoryName.IsNone()))
					{
						FCraftingDecisionTreeNode *chosenForm = node.FindNode([formName, subcategoryName](const FCraftingItem &n)
						{
							return n.PresetFormKey == formName && n.SubCategory == subcategoryName;
						});

						if (chosenForm != nullptr)
						{
							FCraftingDecisionTreeNode *walker = node.FindNodeByGUID(chosenForm->Data.ParentGUID);
							FName guid = chosenForm->Data.GUID;
							while (walker != nullptr)
							{
								if (walker->Data.Type == EDecisionType::Select)
								{
									walker->Data.SelectedSubnodeGUID = guid;
								}
								guid = walker->Data.GUID;
								walker = node.FindNodeByGUID(walker->Data.ParentGUID);
							}
						}

						recurseEntireTree(node, props);

						bool isTrim = props.HasProperty(BIM::EScope::Layer, BIM::Parameters::TrimProfile);

						// TODO: old style portals and trims do not have presets, to be deprecated with new slotted DDL objects
						if (!isTrim && !isPortal && ensureAlways(props.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Preset, presetKey)))
						{
							const FCraftingPreset *preset = GetPresetByKey(*presetKey);
							if (ensureAlways(preset != nullptr))
							{
								FCraftingDecisionTreeNode *formNode = node.FindNode(
									[preset](const FCraftingItem &ci)
								{
									return ci.PresetFormKey == preset->FormName && ci.SubCategory == preset->Subcategory;
								});

								if (ensureAlways(formNode != nullptr))
								{
									UpdateCraftingNodeWithPreset(*formNode, preset->Key);
								}
							}
						}
					}
				}
			}
		}
		return NoError;
	}

	/*
	Get builtins and presets for form
	*/
	FPresetManager::Result FPresetManager::GetBuiltinsForForm(const FName &subcategoryName, const FName &formName, TArray<FCraftingPreset> &presets) const
	{
		for (auto &preset : CraftingBuiltins)
		{
			if (preset.Subcategory == subcategoryName && preset.FormName == formName)
			{
				presets.Add(preset);
			}
		}
		return NoError;
	}

	FPresetManager::Result FPresetManager::GetBuiltinsForForm(EToolMode mode, const Modumate::FCraftingDecisionTree &craftingTree, const FName &formGUID, TArray<FCraftingItem> &outPresets) const
	{
		FCraftingDecisionTree treeCopy = craftingTree;
		const FCraftingDecisionTreeNode *node = GetFormInActiveLayerByGUID(treeCopy, formGUID);

		if (ensureAlways(node != nullptr && node->Data.HasPreset))
		{
			FName formName = node->Data.PresetFormKey;
			FName subcategoryName = node->Data.SubCategory;

			FName familyName = FCraftingPreset::MakePresetFamilyName(subcategoryName, formName);
			TArray<FCraftingPreset> presets;
			GetBuiltinsForForm(subcategoryName, formName, presets);

			int32 dlIndex = treeCopy.RootNode.Data.DynamicListElementIndex;

			for (const auto &p : presets)
			{
				FCraftingItem &newPreset = outPresets.AddDefaulted_GetRef();
				newPreset.Type = EDecisionType::Preset;
				newPreset.Label = FText::FromString(FString::Printf(TEXT("Preset Label  %s %d"), *formGUID.ToString(), 0));
				newPreset.Value = "";
				newPreset.Key = p.Key.ToString();
				newPreset.GUID = NAME_None;
				newPreset.ParentGUID = formGUID;
				newPreset.PresetFormKey = formName;
				newPreset.PresetDisplayName = p.PresetName;
				newPreset.PresetComments = p.Properties.GetValue(BIM::FValueSpec(BIM::EScope::Layer, BIM::EValueType::FixedText, BIM::Parameters::Comments).QN().ToString());
				newPreset.PresetType = ECraftingPresetType::BuiltIn;
			}
			return NoError;
		}
		return Error;
	}

/*
Preset - get, add, remove, update
*/
	FPresetManager::Result FPresetManager::GetPresetsForForm(EToolMode mode, const Modumate::FCraftingDecisionTree &craftingTree, const FName &formGUID, TArray<FCraftingItem> &outPresets) const
	{
		Modumate::FCraftingDecisionTree treeCopy = craftingTree;
		const FCraftingDecisionTreeNode *node = GetFormInActiveLayerByGUID(treeCopy, formGUID);

		if (ensureAlways(node != nullptr && node->Data.HasPreset))
		{
			FName formName = node->Data.PresetFormKey;
			FName subcategoryName = node->Data.SubCategory;

			TArray<FCraftingPreset> presets;
			GetPresetsForForm(subcategoryName, formName, presets);

			int32 dlIndex = treeCopy.RootNode.Data.DynamicListElementIndex;

			for (const auto &p : presets)
			{
				FCraftingItem &newPreset = outPresets.AddDefaulted_GetRef();
				newPreset.Type = EDecisionType::Preset;
				newPreset.Label = FText::FromString(FString::Printf(TEXT("Preset Label  %s %d"), *formGUID.ToString(), 0));
				newPreset.Value = "";
				newPreset.Key = p.Key.ToString();
				newPreset.GUID = NAME_None;
				newPreset.ParentGUID = formGUID;
				newPreset.PresetFormKey = formName;
				newPreset.PresetSequenceCode = p.SequenceCode.ToString();
				newPreset.PresetDisplayName = p.PresetName;
				newPreset.PresetComments = p.Properties.GetValue(BIM::FValueSpec(BIM::EScope::Layer, BIM::EValueType::FixedText, BIM::Parameters::Comments).QN().ToString());
				newPreset.PresetType = ECraftingPresetType::User;
			}
			return NoError;
		}
		return Error;
	}

	FPresetManager::Result FPresetManager::GetPresetsForForm(const FName &subcategoryName, const FName &formName, TArray<FCraftingPreset> &presets) const
	{
		for (auto &preset : CraftingPresetArray)
		{
			if (preset.FormName == formName && preset.Subcategory == subcategoryName)
			{
				presets.Add(preset);
			}
		}
		return NoError;
	}

	const FCraftingPreset *FPresetManager::GetBuiltinByKey(const FName &key) const
	{
		for (const auto &preset : CraftingBuiltins)
		{
			if (preset.Key == key)
			{
				return &preset;
			}
		}
		return nullptr;
	}

	bool FPresetManager::IsBuiltin(const FName &key) const
	{
		for (auto &bi : CraftingBuiltins)
		{
			if (bi.Key == key)
			{
				return true;
			}
		}
		return false;
	}

	const FCraftingPreset *FPresetManager::GetPresetByKey(const FName &key) const
	{
		for (auto &p : CraftingPresetArray)
		{
			if (p.Key == key)
			{
				return &p;
			}
		}
		return nullptr;
	}

	/*
	Find a preset that matches the properties and child presets, used to match existing presets when importing a builtin
	*/
	const FCraftingPreset *FPresetManager::FindMatchingPreset(const FName &subcategory, const FName &form, const BIM::FBIMPropertySheet &properties, const TArray<FName> &childPresets) const
	{
		auto presetMatches = [properties, childPresets, form, subcategory](const FCraftingPreset &preset)
		{
			if (preset.Subcategory == subcategory && preset.FormName == form && preset.ChildPresets.Num() == childPresets.Num())
			{
				// All child presets are the same
				for (int32 i = 0; i < preset.ChildPresets.Num(); ++i)
				{
					if (!preset.ChildPresets.Contains(childPresets[i]))
					{
						return false;
					}
				}

				// All
				bool allPropertiesMatch = true;
				preset.Properties.ForEachProperty([&allPropertiesMatch,properties](const FString &propertyName, const FModumateCommandParameter &param)
				{
					BIM::FValueSpec spec(*propertyName);
					FModumateCommandParameter val;
					if (properties.TryGetProperty(spec.Scope, spec.Name, val))
					{
						if (!val.Equals(param))
						{
							allPropertiesMatch = false;
							return false;
						}
					}
					else
					{
						allPropertiesMatch = false;
						return false;
					}
					return true;
				});
				return allPropertiesMatch;
			}
			return false;
		};

		for (auto &preset : CraftingPresetArray)
		{
			if (presetMatches(preset))
			{
				return &preset;
			}
		}
		return nullptr;
	}

	FPresetManager::Result FPresetManager::DispatchCreateOrUpdatePresetCommand(
		UWorld *world,
		EToolMode mode,
		const BIM::FBIMPropertySheet &properties,
		const TArray<FName> &childPresets,
		const FName &presetKey,
		FName &outPreset)
	{
		TArray<FString> propKeys, propVals;

		properties.ForEachProperty([&propKeys, &propVals](const FString &propName, const FModumateCommandParameter &propVal) {
			propKeys.Add(propName);
			propVals.Add(propVal.AsJSON());
		});

		TArray<FString> childPresetStrings;
		Algo::Transform(childPresets, childPresetStrings, [](const FName &n) {return n.ToString(); });

		FName modeName = FindEnumValueFullName<EToolMode>(TEXT("EToolMode"), mode);
		UModumateGameInstance *gameInstance = Cast<UModumateGameInstance>(world->GetGameInstance());

		FModumateFunctionParameterSet result = gameInstance->DoModumateCommand(
			FModumateCommand(Commands::kCreateOrUpdatePreset_DEPRECATED)
			.Param(Parameters::kKeys, propKeys)
			.Param(Parameters::kValues, propVals)
			.Param(Parameters::kPresetKey, presetKey)
			.Param(Parameters::kChildPresets, childPresetStrings)
			.Param(Parameters::kToolMode, modeName.ToString()));

		outPreset = *result.GetValue(Parameters::kPresetKey).AsString();

		return NoError;
	}

	FPresetManager::Result FPresetManager::HandleCreatePresetCommand(
		EToolMode mode,
		const BIM::FBIMPropertySheet &properties,
		const TArray<FName> &childPresets, 
		FName &outNewPreset)
	{
		/*
		Preset families are identified by subcategory and form
		Forms may exist at multiple layers of the tree but are unique within any subcategory sub-tree
		Subcategory and form are stored within the property sheet defining the preset
		*/

		// TODO: property sheets don't support FName
		FString subcategoryString, formString;
		FName subcategoryName, formName;
		if (ensureAlways(properties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Form, formString) &&
			properties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Subcategory, subcategoryString)))
		{
			subcategoryName = *subcategoryString;
			formName = *formString;

			// Get or create the preset family and add a new preset to it
			FCraftingPreset &newPreset = CraftingPresetArray.AddDefaulted_GetRef();


			// Find an available key by searching the family array for an unused ID
			TArray<FName> keys;
			Algo::Transform(CraftingPresetArray, keys, [](const FCraftingPreset &cp) {return cp.Key; });

			int32 i = 0;
			FString familyName = FCraftingPreset::MakePresetFamilyName(subcategoryName, formName).ToString();
			newPreset.Properties = properties;
			newPreset.FormName = formName;
			newPreset.ToolMode = mode;
			newPreset.ChildPresets = childPresets;
			newPreset.Subcategory = subcategoryName;
			do {
				newPreset.Key = FName(*FString::Printf(TEXT("%s-PRESET%d"), *familyName, ++i));
				newPreset.SequenceCode = FName(*FString::Printf(TEXT("%02d"), i));
			} while (keys.Contains(newPreset.Key));

			// Presets store both the original property sheet and a simple map of FString to FString for serialization purposes
			// The "archive" map must be updated whenever the properties change, which is here and on update
			newPreset.UpdatePresetNameFromProperties();
			newPreset.UpdateArchiveFromProperties();

			outNewPreset = newPreset.Key;
			return NoError;
		}
		return Error;
	}

	FPresetManager::Result FPresetManager::HandleUpdatePresetCommand(
		EToolMode mode,
		const BIM::FBIMPropertySheet &properties, 
		const TArray<FName> &childPresets,
		const FName &presetKey)
	{
		FCraftingPreset *preset = const_cast<FCraftingPreset*>(GetPresetByKey(presetKey));
		if (ensureAlways(preset != nullptr))
		{
			preset->Properties = properties;
			preset->ToolMode = mode;
			preset->ChildPresets = childPresets;
			preset->UpdateArchiveFromProperties();
			return NoError;
		}
		return Error;
	}

	FPresetManager::Result FPresetManager::DispatchRemovePresetCommand(UWorld *world,EToolMode toolMode,const FName &presetKey, const FName &replacementKey)
	{
		FName modeName = FindEnumValueFullName<EToolMode>(TEXT("EToolMode"), toolMode);
		UModumateGameInstance *gameInstance = Cast<UModumateGameInstance>(world->GetGameInstance());

		gameInstance->DoModumateCommand(
			FModumateCommand(Commands::kRemovePreset_DEPRECATED)
			.Param(Parameters::kPresetKey, presetKey)
			.Param(Parameters::kReplacementKey, replacementKey)
			.Param(Parameters::kToolMode, modeName));

		return NoError;
	}

	FPresetManager::Result FPresetManager::HandleRemovePresetCommand(UWorld *world, EToolMode toolMode, const FName &presetKey, const FName &replacementKey)
	{
		const FCraftingPreset *preset = GetPresetByKey(presetKey);
		if (ensureAlways(preset != nullptr))
		{
			FName useReplacement = replacementKey;
			if (useReplacement.IsNone())
			{
				TArray<FCraftingPreset> alternatives;
				GetPresetsForForm(preset->Subcategory, preset->FormName, alternatives);
				if (ensureAlways(alternatives.Num() > 1))
				{
					useReplacement = alternatives[0].Key != presetKey ? alternatives[0].Key : alternatives[1].Key;
				}
			}

			if (ensureAlways(!useReplacement.IsNone()))
			{
				// Get the assembly keys BEFORE we change anything in the preset DB
				TArray<FName> assemblyKeys;
				GetAssembyKeysForPreset(toolMode, presetKey, assemblyKeys);

				// Remove the preset 
				for (int32 i = 0; i < CraftingPresetArray.Num(); ++i)
				{
					if (CraftingPresetArray[i].Key == presetKey)
					{
						CraftingPresetArray.RemoveAt(i);
						break;
					}
				}

				// Update any existing presets to use the replacement
				for (auto &cp : CraftingPresetArray)
				{
					for (auto &childPreset : cp.ChildPresets)
					{
						if (childPreset == presetKey)
						{
							childPreset = useReplacement;
						}
					}
				}

				// And then update all existing assemblies
				for (auto &assemblyKey : assemblyKeys)
				{
					ReplacePresetInAssembly(world, toolMode, assemblyKey, presetKey, replacementKey);
				}

			}
			return NoError;
		}
		return Error;
	}

	FPresetManager::Result FPresetManager::MakeNewPresetFromCraftingTreeAndFormGUID(
		EToolMode mode, 
		Modumate::FCraftingDecisionTree &craftingTree, 
		const FName &formGUID, 
		FCraftingPreset &outPreset) const
	{
		int32 dlIndex = craftingTree.RootNode.Data.DynamicListElementIndex;
		FCraftingDecisionTreeNode *formNode = GetFormInActiveLayerByGUID(craftingTree, formGUID);

		if (ensureAlways(formNode != nullptr))
		{
			BIM::FBIMPropertySheet properties;
			TArray<FName> childPresets;
			GatherNodePresetProperties(*formNode, outPreset.Properties, outPreset.ChildPresets);
			outPreset.Subcategory = formNode->Data.SubCategory;
			outPreset.ToolMode = mode;
			outPreset.FormName = formNode->Data.PresetFormKey;

			if (!outPreset.Properties.TryGetProperty(BIM::EScope::Preset, BIM::Parameters::Name, outPreset.PresetName))
			{
				outPreset.PresetName = TEXT("Unnamed Preset");
			};

			return NoError;
		}
		return Error;
	}

	/*
	Data gathering
	*/
	FPresetManager::Result FPresetManager::TryGetPresetPropertiesRecurse(const FName &presetKey, BIM::FBIMPropertySheet &props) const
	{
		Result ret = NotFound;
		TArray<FName> keyStack;
		keyStack.Push(presetKey);
		while (keyStack.Num() > 0)
		{
			const FCraftingPreset *preset = GetPresetByKey(keyStack.Pop());
			if (preset != nullptr)
			{
				preset->Properties.ForEachProperty([&props](const FString &propName, const FModumateCommandParameter &mcp) {
					BIM::FValueSpec vs(*propName);
					props.SetProperty(vs.Scope, vs.Name, mcp);
				});

				for (auto &child : preset->ChildPresets)
				{
					keyStack.Push(child);
				}

				ret = Found;
			}
		}
		return ret;
	}

	FPresetManager::Result  FPresetManager::GatherCraftingProperties(
		const Modumate::FCraftingDecisionTree &craftingTree,
		BIM::FModumateAssemblyPropertySpec &propertySheet,
		bool usePreset)
	{
		auto setPropertiesForItem = [](const FCraftingItem &item, BIM::FBIMPropertySheet &ps)
		{
			item.Properties.ForEachProperty([&ps](const FString &name, const FModumateCommandParameter &param)
			{
				BIM::FValueSpec v(*name, param);
				// The "node" scope is only used for internal crafting logic, not part of the final assembly
				if (v.Scope != BIM::EScope::Node && v.Scope != BIM::EScope::Error)
				{
					ps.SetProperty(v.Scope, v.Name, param);
				}
			});
		};

		craftingTree.RootNode.GetTrace(
			[&propertySheet, setPropertiesForItem](const TDecisionTreeNode<FCraftingItem> &item)
		{
			setPropertiesForItem(item.Data, propertySheet.RootProperties);
		}
		);

		for (auto &kvp : craftingTree.DynamicListElementInstances)
		{
			for (auto &le : kvp.Value)
			{
				BIM::FBIMPropertySheet &ps = propertySheet.LayerProperties.AddDefaulted_GetRef();
				FName preset = NAME_None;
				bool wantPreset = true;
				le.GetTrace(
					[&preset, &ps, &wantPreset, setPropertiesForItem](const TDecisionTreeNode<FCraftingItem> &item)
				{
					setPropertiesForItem(item.Data, ps);
					if (wantPreset)
					{
						if (item.Data.HasPreset)
						{
							preset = item.Data.SelectedPreset;
							wantPreset = false;
						}
					}
				}
				);

				// If we found a preset...
				if (!preset.IsNone())
				{
					// Normally, presets dominate, so clear out other properties and just set the preset
					if (usePreset)
					{
						ps.Empty();
						ps.SetProperty(BIM::EScope::Layer, BIM::Parameters::Preset, preset);
					}
					// But if we're doing a preview update, we want to ignore the preset, so throw it away
					else
					{
						ps.RemoveProperty(BIM::EScope::Layer, BIM::Parameters::Preset);
					}
				}
			}
		}
		return NoError;
	}

	FPresetManager::Result FPresetManager::ForEachChildPreset(const FName &presetKey, std::function<void(const FName &name)> fn) const
	{
		TArray<FName> presetStack;
		presetStack.Push(presetKey);
		while (presetStack.Num() != 0)
		{
			FName pkey = presetStack.Pop();

			fn(pkey);

			const FCraftingPreset *pPreset = GetPresetByKey(pkey);
			if (pPreset != nullptr)
			{
				for (auto &p : pPreset->ChildPresets)
				{
					presetStack.Push(p);
				}
			}
		}
		return NoError;
	}

	FPresetManager::Result FPresetManager::GetChildPresets(const FName &presetKey, TArray<FName> &children) const
	{
		return ForEachChildPreset(presetKey, [&children](const FName &pk) {children.AddUnique(pk); });
	}

	FPresetManager::Result FPresetManager::ForEachDependentPreset(const FName &presetKey, std::function<void(const FName &name)> fn) const
	{
		TArray<FName> presetStack;
		presetStack.Push(presetKey);
		while (presetStack.Num() != 0)
		{
			FName pkey = presetStack.Pop();
			for (auto &p : CraftingPresetArray)
			{
				if (p.ChildPresets.Contains(pkey))
				{
					fn(p.Key);
					presetStack.Push(p.Key);
				}
			}
		}
		return NoError;
	}

	FPresetManager::Result FPresetManager::GetDependentPresets(const FName &presetKey, TArray<FName> &dependents) const
	{
		return ForEachDependentPreset(presetKey, [&dependents](const FName &pk) {dependents.AddUnique(pk); });
	}

	FPresetManager::Result FPresetManager::GatherNodePresetProperties(const FCraftingDecisionTreeNode &formNode, BIM::FBIMPropertySheet &properties, TArray<FName> &childPresets) const
	{
		FName formName = formNode.Data.PresetFormKey;
		FName subcategoryName = formNode.Data.SubCategory;
		if (ensureAlways(!formName.IsNone() && !subcategoryName.IsNone()))
		{
			properties.SetProperty(BIM::EScope::Layer, BIM::Parameters::Form, formName);
			properties.SetProperty(BIM::EScope::Layer, BIM::Parameters::Subcategory, subcategoryName);
		}

		TArray<const FCraftingDecisionTreeNode*> nodeStack;
		nodeStack.Push(&formNode);
		while (nodeStack.Num() > 0)
		{
			const FCraftingDecisionTreeNode *iterNode = nodeStack.Pop();
			iterNode->Data.Properties.ForEachProperty([&properties](const FString &name, const FModumateCommandParameter &prop) {
				BIM::FValueSpec vs(*name);
				if (!properties.HasProperty(vs.Scope, vs.Name))
				{
					properties.SetValue(name, prop);
				}
			});

			for (auto &prop : iterNode->Data.PropertyValueBindings)
			{
				properties.SetValue(prop, iterNode->Data.Value);
			}

			if (iterNode->Data.Type == EDecisionType::Select)
			{
				nodeStack.Push(iterNode->GetSelectedSubfield());
			}
			else
			{
				for (auto &sf : iterNode->Subfields)
				{
					// Don't assimilate children with their own presets
					if (!sf.Data.HasPreset)
					{
						nodeStack.Push(&sf);
					}
					else
					{
						if (ensureAlways(!sf.Data.SelectedPreset.IsNone()))
						{
							childPresets.Add(sf.Data.SelectedPreset);
						}
					}
				}
			}
		}
		return NoError;
	}

	/*
	Tree manipulation
	*/

	FPresetManager::Result FPresetManager::UpdateCraftingNodeWithPreset(FCraftingDecisionTreeNode &rootNode,const FName &presetKey) const
	{
		FCraftingDecisionTreeNode *node = &rootNode;

		if (ensureAlways(node != nullptr && node->Data.HasPreset))
		{
			TArray<FCraftingDecisionTreeNode *> nodeStack;

			node->Data.SelectedPreset = presetKey;
			nodeStack.Push(node);
			while (nodeStack.Num() > 0)
			{
				node = nodeStack.Pop();
				const FCraftingPreset *preset = GetPresetByKey(node->Data.SelectedPreset);

				// Builtins carry properties all the way down and presets are limited to just their own properties, so go deep if it's a builtin
				bool isBuiltin = false;
				if (preset == nullptr)
				{
					preset = GetBuiltinByKey(node->Data.SelectedPreset);
					node->Data.SelectedPreset = NAME_None;
					isBuiltin = true;
				}

				if (ensureAlways(preset != nullptr))
				{
					node->Data.Value = preset->SequenceCode.ToString();
					if (isBuiltin)
					{
						recurseEntireTree(*node, preset->Properties);
					}
					else
					{
						recursePresetForm(*node, preset->Properties);
					}
					for (auto &childPreset : preset->ChildPresets)
					{
						preset = GetPresetByKey(childPreset);
						if (preset != nullptr)
						{
							FCraftingDecisionTreeNode *newNode = GetChildNodeByPreset(*node, *preset);
							if (ensureAlways(newNode != nullptr))
							{
								newNode->Data.SelectedPreset = preset->Key;
								nodeStack.Push(newNode);
							}
						}
					}
				}
				else
				{
					return Error;
				}
			}
		}
		else
		{
			return Error;
		}
		return NoError;
	}

	FPresetManager::Result FPresetManager::UpdateCraftingTreeWithPreset(FCraftingDecisionTree &craftingTree, const FName &formGUID, const FName &presetKey) const
	{
		FCraftingDecisionTreeNode *node = GetFormInActiveLayerByGUID(craftingTree, formGUID);
		return UpdateCraftingNodeWithPreset(*node, presetKey);
	}

	FPresetManager::Result FPresetManager::UpdateCraftingTreeWithBuiltin(UWorld *world,EToolMode mode, FCraftingDecisionTree &craftingTree, const FName &formGUID, const FName &presetKey)
	{
		const FCraftingPreset *preset = GetBuiltinByKey(presetKey);
		FCraftingDecisionTreeNode *node = GetFormInActiveLayerByGUID(craftingTree, formGUID);
		if (ensureAlways(preset != nullptr && node != nullptr))
		{
			recurseEntireTree(*node, preset->Properties);
			FindOrCreatePresets(world,mode, craftingTree);
		}
		return NoError;
	}

	Modumate::FCraftingDecisionTreeNode *FPresetManager::GetFormInActiveLayerByGUID(Modumate::FCraftingDecisionTree &craftingTree, const FName &formGUID)
	{
		int32 dlIndex = craftingTree.RootNode.Data.DynamicListElementIndex;
		FCraftingDecisionTree::FDecisionNodeArray *instances = craftingTree.DynamicListElementInstances.Find(craftingTree.RootNode.Data.DynamicList);

		if (ensureAlways(instances != nullptr && dlIndex < instances->Num()))
		{
			return (*instances)[dlIndex].FindNodeByGUID(formGUID);
		}
		return nullptr;
	}

	FCraftingDecisionTreeNode *FPresetManager::GetChildNodeByPreset(FCraftingDecisionTreeNode &craftingNode, const FCraftingPreset &preset)
	{
		return craftingNode.FindNode([preset](const FCraftingItem &ci) {return ci.PresetFormKey == preset.FormName && ci.SubCategory == preset.Subcategory; });
	}

	/*
	Assembly management
	*/

	FPresetManager::Result FPresetManager::BuildAssemblyFromCraftingTree(
		UWorld *world,
		EToolMode toolMode,
		const FCraftingDecisionTree &craftingTree,
		FModumateObjectAssembly &outAssembly,
		bool usesPresets,
		const int32 showOnlyLayerID)
		const
	{
		BIM::FModumateAssemblyPropertySpec spec;
		GatherCraftingProperties(craftingTree, spec, usesPresets);
		spec.ObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(toolMode);
		AEditModelGameMode_CPP *mode = Cast<AEditModelGameMode_CPP>(world->GetAuthGameMode());
		UModumateObjectAssemblyStatics::DoMakeAssembly(*(mode->ObjectDatabase), *this, spec, outAssembly, showOnlyLayerID);
		return NoError;
	}

	FPresetManager::Result FPresetManager::GetAssembyKeysForPreset(EToolMode mode, const FName &presetKey, TArray<FName> &assemblyKeys) const
	{
		const DataCollection<FModumateObjectAssembly> *db = AssemblyDBs_DEPRECATED.Find(mode);

		if (ensureAlways(db != nullptr))
		{
			TArray<FName> keys;
			GetDependentPresets(presetKey, keys);
			keys.Add(presetKey);

			for (auto &key : keys)
			{
				for (auto &kvp : db->DataMap)
				{
					if (kvp.Value.UsesPreset_DEPRECATED(key))
					{
						assemblyKeys.Add(kvp.Key);
					}
				}
			}
			return NoError;
		}
		return Error;
	}

	FPresetManager::Result FPresetManager::RefreshExistingAssembly(UWorld *world, EToolMode mode, const FName &assemblyKey)
	{
		const FModumateObjectAssembly *pAsm = GetAssemblyByKey(mode, assemblyKey);
		if (ensureAlways(pAsm != nullptr))
		{
			BIM::FModumateAssemblyPropertySpec spec;
			pAsm->FillSpec(spec);
			FModumateObjectAssembly updatedAsm;

			AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
			UModumateObjectAssemblyStatics::DoMakeAssembly(*(gameMode->ObjectDatabase), *this, spec, updatedAsm);
			updatedAsm.DatabaseKey = assemblyKey;

			DataCollection<FModumateObjectAssembly>  *db = AssemblyDBs_DEPRECATED.Find(mode);
			if (ensureAlways(db != nullptr))
			{
				const FModumateObjectAssembly *pOldAsm = db->GetData(assemblyKey);
				if (ensureAlways(pOldAsm != nullptr))
				{
					db->RemoveData(*pOldAsm);
					db->AddData(updatedAsm);
				}
			}

			AEditModelGameState_CPP *gameState = world->GetGameState<AEditModelGameState_CPP>();
			gameState->Document.OnAssemblyUpdated(world, mode, updatedAsm);
		}
		return NoError;
	}

	FPresetManager::Result FPresetManager::ReplacePresetInAssembly(UWorld *world, EToolMode mode, const FName &assemblyKey, const FName &oldPreset, const FName &newPreset)
	{
		DataCollection<FModumateObjectAssembly>  *db = AssemblyDBs_DEPRECATED.Find(mode);
		if (ensureAlways(db != nullptr))
		{
			const FModumateObjectAssembly *pOldAsm = db->GetData(assemblyKey);
			if (ensureAlways(pOldAsm != nullptr))
			{
				FModumateObjectAssembly newAsm = *pOldAsm;
				newAsm.ReplacePreset_DEPRECATED(oldPreset, newPreset);
				db->RemoveData(*pOldAsm);
				db->AddData(newAsm);
				RefreshExistingAssembly(world, mode, assemblyKey);
			}
		}
		return NoError;
	}

	const FModumateObjectAssembly *FPresetManager::GetAssemblyByKey(const FName &Key) const
	{
		for (auto &kvp : AssemblyDBs_DEPRECATED)
		{
			const FModumateObjectAssembly *ret = GetAssemblyByKey(kvp.Key, Key);
			if (ret != nullptr)
			{
				return ret;
			}
		}
		return nullptr;
	}

	const FModumateObjectAssembly *FPresetManager::GetAssemblyByKey(EToolMode Mode, const FName &Key) const
	{
		EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(Mode);
		if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(objectType))
		{
			const FAssemblyDataCollection *db = AssembliesByObjectType.Find(objectType);
			if (db != nullptr)
			{
				return db->GetData(Key);
			}
		}

		const DataCollection<FModumateObjectAssembly> *db = AssemblyDBs_DEPRECATED.Find(Mode);

		if (ensureAlways(db != nullptr))
		{
			return db->GetData(Key);
		}
		return nullptr;
	}

	ECraftingResult FPresetManager::FetchAllPresetsForObjectType(EObjectType ObjectType, TSet<FName> &OutPresets) const
	{
		if (!ensureAlways(UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(ObjectType)))
		{
			return ECraftingResult::Error;
		}
		const DataCollection<FModumateObjectAssembly> *db = AssembliesByObjectType.Find(ObjectType);
		
		if (db == nullptr)
		{
			// It's legal for there to be no presets for a type
			return ECraftingResult::Success;
		}

		for (auto &kvp : db->DataMap)
		{
			OutPresets.Add(kvp.Value.RootPreset);
			CraftingNodePresets.GetDependentPresets(kvp.Value.RootPreset, OutPresets);
		}

		return ECraftingResult::Success;
	}


	//Gathers all the properties needed to build an assembly into a property sheet
	ECraftingResult FPresetManager::PresetToSpec(const FName &PresetID, BIM::FModumateAssemblyPropertySpec &OutPropertySpec) const
	{
		ECraftingResult ret = ECraftingResult::Success;
		OutPropertySpec.RootPreset = PresetID;

		BIM::FBIMPropertySheet *currentSheet = &OutPropertySpec.RootProperties;

		TArray<FName> presetStack;
		presetStack.Push(PresetID);

		TArray<EBIMValueScope> scopeStack;
		scopeStack.Push(EBIMValueScope::Assembly);

		while (presetStack.Num() > 0)
		{
			FName presetID = presetStack.Pop();
			EBIMValueScope pinScope = scopeStack.Pop();

			const BIM::FCraftingTreeNodePreset *preset = CraftingNodePresets.Presets.Find(presetID);
			if (preset == nullptr)
			{
				ret = ECraftingResult::Error;
			}
			else
			{
				// Nodes with a layer function constitute a new layer
				if (preset->Properties.HasProperty(BIM::EScope::Layer, BIM::Parameters::Function))
				{
					OutPropertySpec.LayerProperties.AddDefaulted();
					currentSheet = &OutPropertySpec.LayerProperties.Last();
				}

				EObjectType objectType = CraftingNodePresets.GetPresetObjectType(presetID);
				if (objectType != EObjectType::OTNone && ensureAlways(OutPropertySpec.ObjectType == EObjectType::OTNone))
				{
					OutPropertySpec.ObjectType = objectType;
				}

				preset->Properties.ForEachProperty([&OutPropertySpec, &currentSheet, &preset,pinScope,PresetID](const FString &Name, const FModumateCommandParameter &MCP)
				{
					BIM::FValueSpec vs(*Name);

					// 'Node' scope values inherit their scope from their parents, specified on the pin
					BIM::EScope nodeScope = vs.Scope == BIM::EScope::Node ? pinScope : vs.Scope;

					// Preset properties only apply to the preset itself, not to its children
					if (nodeScope != BIM::EScope::Preset || preset->PresetID == PresetID)
					{
						currentSheet->SetProperty(nodeScope, vs.Name, MCP);
					}
				});

				for (auto &cn : preset->ChildNodes)
				{
					// Only the last preset in a sequence contains pertinent data...the others are just category selectors
					if (ensureAlways(cn.PresetSequence.Num() > 0))
					{
						presetStack.Push(cn.PresetSequence.Last().SelectedPresetID);
					}
					scopeStack.Push(cn.Scope != BIM::EScope::None ? cn.Scope : pinScope);
				}
			}
		}

		// All assembly specs must bind to an object type
		if (OutPropertySpec.ObjectType == EObjectType::OTNone)
		{
			return ECraftingResult::Error;
		}
		
		return ret;
	}

	ECraftingResult FPresetManager::MakeNewOrUpdateExistingPresetFromParameterSet(UWorld *World,const FModumateFunctionParameterSet &ParameterSet,FName &OutKey)
	{
		AEditModelGameMode_CPP *mode = Cast<AEditModelGameMode_CPP>(World->GetAuthGameMode());
		Modumate::ModumateObjectDatabase &objectDB = *mode->ObjectDatabase;

		BIM::FCraftingTreeNodePreset preset;
		ECraftingResult result = preset.FromParameterSet(CraftingNodePresets,ParameterSet);

		if (result != ECraftingResult::Success)
		{
			return result;
		}

		if (preset.PresetID.IsNone())
		{
			FName baseKey = ParameterSet.GetValue(Modumate::Parameters::kBaseKeyName);
			if (!ensureAlways(!baseKey.IsNone()))
			{
				return ECraftingResult::Error;
			}
			preset.PresetID = GetAvailableKey(baseKey);
		}
		CraftingNodePresets.Presets.Add(preset.PresetID, preset);

		EObjectType objectType = CraftingNodePresets.GetPresetObjectType(preset.PresetID);

		// if this preset is bound to an object type, create or update its associated project assembly
		if (UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(objectType))
		{
			FModumateObjectAssembly newAssembly;
			BIM::FModumateAssemblyPropertySpec spec;
			PresetToSpec(preset.PresetID, spec);
			result = UModumateObjectAssemblyStatics::DoMakeAssembly(objectDB,*this,spec,newAssembly);
			if (result != ECraftingResult::Success)
			{
				return result;
			}
			// TODO: these will be merged in DDL 2.0
			newAssembly.DatabaseKey = newAssembly.RootPreset;

			FAssemblyDataCollection &assemblyDB = AssembliesByObjectType.FindOrAdd(objectType);
			assemblyDB.AddData(newAssembly);

			AEditModelGameState_CPP *gameState = World->GetGameState<AEditModelGameState_CPP>();
			EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(objectType);
			gameState->Document.OnAssemblyUpdated(World, toolMode, newAssembly);
		}

		OutKey = preset.PresetID;

		return ECraftingResult::Success;
	}


}