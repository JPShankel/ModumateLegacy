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
	FString newKey;
	int32 num = 0;
	do
	{
		newKey = FString::Printf(TEXT("%s-%d"), *BaseKey.ToString(), ++num);
	} while (KeyStore.Contains(newKey));

	KeyStore.Add(newKey);
	return newKey;
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
	CraftingNodePresets = DocRecord.PresetCollection;
	KeyStore = DocRecord.KeyStore;
	AssembliesByObjectType.Empty();
	for (auto& kvp : CraftingNodePresets.Presets)
	{
		FBIMPresetTypeDefinition* typeDef = CraftingNodePresets.NodeDescriptors.Find(kvp.Value.NodeType);
		if (ensureAlways(typeDef != nullptr))
		{
			kvp.Value.TypeDefinition = *typeDef;
		}

		if (kvp.Value.ObjectType != EObjectType::OTNone)
		{
			FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(kvp.Value.ObjectType);
			FBIMAssemblySpec newSpec;
			newSpec.FromPreset(InDB, CraftingNodePresets, kvp.Key);
			db.AddData(newSpec);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FPresetManager::ToDocumentRecord(FMOIDocumentRecord& DocRecord) const
{
	DocRecord.PresetCollection = CraftingNodePresets;
	DocRecord.KeyStore = KeyStore;
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

	/*
	If we don't have a parent, we're an object assembly, so return all compatible object assemblies
	*/
	if (ParentPresetID.IsNone())
	{
		if (!ensureAlways(preset->ObjectType != EObjectType::OTNone))
		{
			return EBIMResult::Error;
		}
		for (auto& candidate : CraftingNodePresets.Presets)
		{
			if (candidate.Value.NodeType == preset->NodeType && candidate.Value.ObjectType == preset->ObjectType)
			{
				OutAvailablePresets.Add(candidate.Value.PresetID);
			}
		}
	}
	else // Otherwise, find all presets of same type that are supported by parent as indicated by tags
	{
		const FBIMPresetInstance* parentPreset = CraftingNodePresets.Presets.Find(ParentPresetID);
		if (!ensureAlways(parentPreset != nullptr))
		{
			return EBIMResult::Error;
		}

		OutAvailablePresets.Empty();
		for (auto& candidate : CraftingNodePresets.Presets)
		{
			if (candidate.Value.NodeType == preset->NodeType && parentPreset->SupportsChild(candidate.Value))
			{
				OutAvailablePresets.Add(candidate.Value.PresetID);
			}
		}
	}

	return EBIMResult::Success;
}


