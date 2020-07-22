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
		//TODO: deactivated for DDL 2.0 refactor, crafting disabled 
		return ECraftingResult::Success;
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
		//TODO: deactivated for DDL 2.0 refactor, crafting disabled 
		return ECraftingResult::Success;
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
		const TModumateDataCollection<FModumateObjectAssembly> *db = AssembliesByObjectType.Find(ObjectType);

		// It's legal for an object database to not exist
		if (db == nullptr)
		{
			OutAssembly = nullptr;
			return false;
		}

		OutAssembly = db->GetData(PresetID);

		// TODO: temp patch to support missing assemblies prior to assemblies being retired
		if (OutAssembly == nullptr)
		{
			OutAssembly = db->GetData(TEXT("default"));
		}

		return (OutAssembly != nullptr);
	}

	FPresetManager::Result FPresetManager::InitAssemblyDBs()
	{
		AssemblyDBs_DEPRECATED.Reset();
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_WALL, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_FLOOR, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_DOOR, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_WINDOW, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_STAIR, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_RAIL, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_CABINET, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_COUNTERTOP, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_FINISH, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_PLACEOBJECT, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_TRIM, TModumateDataCollection<FModumateObjectAssembly>());
		AssemblyDBs_DEPRECATED.Add(EToolMode::VE_ROOF_FACE, TModumateDataCollection<FModumateObjectAssembly>());

		AssembliesByObjectType.Add(EObjectType::OTStructureLine, TModumateDataCollection<FModumateObjectAssembly>());
		AssembliesByObjectType.Add(EObjectType::OTRoofPerimeter, TModumateDataCollection<FModumateObjectAssembly>());

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
			else
			{
				return nullptr;
			}
		}

		const TModumateDataCollection<FModumateObjectAssembly> *db = AssemblyDBs_DEPRECATED.Find(Mode);

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
		const TModumateDataCollection<FModumateObjectAssembly> *db = AssembliesByObjectType.Find(ObjectType);
		
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
				const BIM::FCraftingTreeNodeType *nodeType = CraftingNodePresets.NodeDescriptors.Find(preset->NodeType);
				if (ensureAlways(nodeType != nullptr))
				{
					if (nodeType->Scope == EBIMValueScope::Layer)
					{
						OutPropertySpec.LayerProperties.AddDefaulted();
						currentSheet = &OutPropertySpec.LayerProperties.Last();
					}
					if (nodeType->Scope != EBIMValueScope::None)
					{
						pinScope = nodeType->Scope;
					}
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

				for (auto &childPreset : preset->ChildPresets)
				{
					presetStack.Push(childPreset.PresetID);
					scopeStack.Push(pinScope);
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
		FModumateDatabase &objectDB = *mode->ObjectDatabase;

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