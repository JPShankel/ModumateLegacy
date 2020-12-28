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

FBIMKey FPresetManager::GetAvailableKey(const FGuid& BaseKey)
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

	// Old file support
	if (DocRecord.PresetCollection.PresetsByGUID.Num() == 0)
	{
		for (auto& kvp : DocRecord.PresetCollection.Presets_DEPRECATED)
		{
			if (!CraftingNodePresets.PresetsByGUID.Contains(kvp.Value.GUID))
			{
				FBIMPresetInstance editedPreset = kvp.Value;
				editedPreset.ReadOnly = false;
				CraftingNodePresets.AddPreset(editedPreset);
			}
		}
	}
	else 
	{
		for (auto& kvp : DocRecord.PresetCollection.PresetsByGUID)
		{
			if (!CraftingNodePresets.PresetsByGUID.Contains(kvp.Key))
			{
				FBIMPresetInstance editedPreset = kvp.Value;
				editedPreset.ReadOnly = false;
				CraftingNodePresets.AddPreset(editedPreset);
			}
		}
	}
	
	// If any presets fail to build their assembly, remove them
	// They'll be replaced by the fallback system and will not be written out again
	TSet<FGuid> incompletePresets;
	for (auto& kvp : CraftingNodePresets.PresetsByGUID)
	{
		if (kvp.Value.ObjectType != EObjectType::OTNone)
		{
			FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(kvp.Value.ObjectType);
			FBIMAssemblySpec newSpec;
			if (newSpec.FromPreset(InDB, CraftingNodePresets, kvp.Value.GUID) == EBIMResult::Success)
			{
				db.AddData(newSpec);
			}
			else
			{
				incompletePresets.Add(kvp.Value.GUID);
			}
		}
	}

	for (auto& incompletePreset : incompletePresets)
	{
		CraftingNodePresets.RemovePreset(incompletePreset);
	}

	return CraftingNodePresets.PostLoad();
}

EBIMResult FPresetManager::ToDocumentRecord(FMOIDocumentRecord& DocRecord) const
{
	for (auto& kvp : CraftingNodePresets.PresetsByGUID)
	{
		// ReadOnly presets have not been edited
		if (!kvp.Value.ReadOnly)
		{
			DocRecord.PresetCollection.AddPreset(kvp.Value);
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

EBIMResult FPresetManager::RemoveProjectAssemblyForPreset(const FGuid& PresetID)
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

bool FPresetManager::TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FGuid& PresetID, FBIMAssemblySpec& OutAssembly) const
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

const FBIMAssemblySpec *FPresetManager::GetAssemblyByGUID(EToolMode ToolMode, const FGuid& Key) const
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

EBIMResult FPresetManager::GetAvailablePresetsForSwap(const FGuid& ParentPresetID, const FGuid&PresetIDToSwap, TArray<FGuid>& OutAvailablePresets)
{
	const FBIMPresetInstance* preset = CraftingNodePresets.PresetFromGUID(PresetIDToSwap);
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


