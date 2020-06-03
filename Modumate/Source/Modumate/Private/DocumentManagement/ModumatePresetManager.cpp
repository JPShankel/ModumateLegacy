// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumatePresetManager.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Database/ModumateObjectEnums.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Algo/Transform.h"

using namespace Modumate;

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

	
	/*
	Initializers
	*/

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

	bool FPresetManager::TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FModumateObjectAssembly &OutAssembly) const
	{
		EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);
		const FAssemblyDataCollection *db = AssembliesByObjectType.Find(objectType);
		if (db != nullptr && db->DataMap.Num() > 0)
		{
			auto iterator = db->DataMap.CreateConstIterator();
			OutAssembly = iterator->Value;
			return true;
		}
		return false;
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

		return NoError;
	}



/*
Starting point: get a tree
*/

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