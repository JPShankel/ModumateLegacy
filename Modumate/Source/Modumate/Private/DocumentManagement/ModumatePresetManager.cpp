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

FPresetManager::FPresetManager()
{}

FPresetManager::~FPresetManager()
{}

FBIMKey FPresetManager::GetAvailableKey(const FBIMKey& BaseKey)
{
	FBIMKey outKey;
	CraftingNodePresets.GenerateBIMKeyForPreset(BaseKey, outKey);
	return outKey;
}

EBIMResult FPresetManager::GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const
{
	const FAssemblyDataCollection* db = AssembliesByObjectType.Find(ObjectType);
	if (db == nullptr)
	{
		return EBIMResult::Success;
	}
	for (auto& kvp : db->DataMap)
	{
		OutAssemblies.Add(kvp.Value);
	}
	return EBIMResult::Success;
}

EBIMResult FPresetManager::FromDocumentRecord(const FModumateDatabase& InDB, const FMOIDocumentRecord& DocRecord)
{
	for (auto& kvp : DocRecord.PresetCollection.NodeDescriptors)
	{
		if (!CraftingNodePresets.NodeDescriptors.Contains(kvp.Key))
		{
			CraftingNodePresets.NodeDescriptors.Add(kvp.Key, kvp.Value);
		}
	}

	for (auto& kvp : DocRecord.PresetCollection.Presets)
	{
		if (!CraftingNodePresets.Presets.Contains(kvp.Key))
		{
			FBIMPresetInstance editedPreset = kvp.Value;
			editedPreset.ReadOnly = false;
			CraftingNodePresets.Presets.Add(kvp.Key, editedPreset);
		}
	}

	AssembliesByObjectType.Empty();
	CraftingNodePresets.PostLoad();
	
	// If any presets fail to build their assembly, remove them
	// They'll be replaced by the fallback system and will not be written out again
	TSet<FBIMKey> incompletePresets;
	for (auto& kvp : CraftingNodePresets.Presets)
	{
		if (kvp.Value.ObjectType != EObjectType::OTNone)
		{
			FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(kvp.Value.ObjectType);
			FBIMAssemblySpec newSpec;
			if (newSpec.FromPreset(InDB, CraftingNodePresets, kvp.Key) == EBIMResult::Success)
			{
				db.AddData(newSpec);
			}
			else
			{
				incompletePresets.Add(kvp.Key);
			}
		}
	}

	for (auto& incompletePreset : incompletePresets)
	{
		CraftingNodePresets.Presets.Remove(incompletePreset);
	}

	return EBIMResult::Success;
}

EBIMResult FPresetManager::ToDocumentRecord(FMOIDocumentRecord& DocRecord) const
{
	for (auto& kvp : CraftingNodePresets.Presets)
	{
		// ReadOnly presets have not been edited
		if (!kvp.Value.ReadOnly)
		{
			DocRecord.PresetCollection.Presets.Add(kvp.Key, kvp.Value);
			if (!DocRecord.PresetCollection.NodeDescriptors.Contains(kvp.Value.NodeType))
			{
				const FBIMPresetTypeDefinition* typeDef = CraftingNodePresets.NodeDescriptors.Find(kvp.Value.NodeType);
				if (ensureAlways(typeDef != nullptr))
				{
					DocRecord.PresetCollection.NodeDescriptors.Add(kvp.Value.NodeType, *typeDef);
				}
			};
		}
	}

	return EBIMResult::Success;
}

EBIMResult FPresetManager::AddOrUpdateGraph2DRecord(const FBIMKey& Key, const FGraph2DRecord& Graph, FBIMKey& OutKey)
{
	static const FString defaultGraphBaseKey(TEXT("Graph2D"));
	OutKey = GetAvailableKey(defaultGraphBaseKey);
	return EBIMResult::Success;
}

EBIMResult FPresetManager::GetGraph2DRecord(const FBIMKey& Key, FGraph2DRecord& OutGraph) const
{
	if (GraphCollection.Contains(Key))
	{
		OutGraph = GraphCollection.FindChecked(Key);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FPresetManager::RemoveGraph2DRecord(const FBIMKey& Key)
{
	int32 numRemoved = GraphCollection.Remove(Key);
	return (numRemoved > 0) ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FPresetManager::RemoveProjectAssemblyForPreset(const FBIMKey& PresetID)
{
	FBIMAssemblySpec assembly;
	EObjectType objectType = CraftingNodePresets.GetPresetObjectType(PresetID);
	if (TryGetProjectAssemblyForPreset(objectType, PresetID, assembly))
	{
		FAssemblyDataCollection* db = AssembliesByObjectType.Find(objectType);
		if (ensureAlways(db != nullptr))
		{
			db->RemoveData(assembly);
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FPresetManager::UpdateProjectAssembly(const FBIMAssemblySpec& Assembly)
{
	FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(Assembly.ObjectType);
	db.AddData(Assembly);
	return EBIMResult::Success;
}

bool FPresetManager::TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const
{
	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);
	const FAssemblyDataCollection* db = AssembliesByObjectType.Find(objectType);
	if (db != nullptr && db->DataMap.Num() > 0)
	{
		auto iterator = db->DataMap.CreateConstIterator();
		OutAssembly = iterator->Value;
		return true;
	}
	return false;
}

bool FPresetManager::TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FBIMKey& PresetID, FBIMAssemblySpec& OutAssembly) const
{
	const TModumateDataCollection<FBIMAssemblySpec> *db = AssembliesByObjectType.Find(ObjectType);

	// It's legal for an object database to not exist
	if (db == nullptr)
	{
		return false;
	}

	const FBIMAssemblySpec* presetAssembly = db->GetData(PresetID);

	// TODO: temp patch to support missing assemblies prior to assemblies being retired
	if (presetAssembly == nullptr)
	{
		presetAssembly = DefaultAssembliesByObjectType.Find(ObjectType);
	}

	if (presetAssembly != nullptr)
	{
		OutAssembly = *presetAssembly;
		return true;
	}
	return false;
}

const FBIMAssemblySpec *FPresetManager::GetAssemblyByKey(EToolMode ToolMode, const FBIMKey& Key) const
{
	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);
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

EBIMResult FPresetManager::GetAvailablePresetsForSwap(const FBIMKey& ParentPresetID, const FBIMKey &PresetIDToSwap, TArray<FBIMKey>& OutAvailablePresets)
{
	const FBIMPresetInstance* preset = CraftingNodePresets.Presets.Find(PresetIDToSwap);
	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	return CraftingNodePresets.GetPresetsByPredicate([preset](const FBIMPresetInstance& Preset)
		{
			return Preset.MyTagPath.MatchesExact(preset->MyTagPath);
		}, OutAvailablePresets);


	return EBIMResult::Success;
}


