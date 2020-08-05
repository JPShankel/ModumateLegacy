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

ECraftingResult FPresetManager::FromDocumentRecord(UWorld* World, const FModumateDocumentHeader& DocumentHeader, const FMOIDocumentRecord& DocumentRecord)
{
	KeyStore = DocumentRecord.KeyStore;
	CraftingNodePresets.FromDataRecords(DocumentRecord.CraftingPresetArrayV2);

	FBIMPresetCollection &presetCollection = CraftingNodePresets;

	// In the DDL2verse, assemblies are stored solely by their root preset ID then rebaked into runtime assemblies on load
	for (auto& presetID : DocumentRecord.ProjectAssemblyPresets)
	{
		// this ensure will fire if expected presets have become obsolete, resave to fix
		if (!ensureAlways(CraftingNodePresets.Presets.Contains(presetID)))
		{
			continue;
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

ECraftingResult FPresetManager::GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const
{
	const FAssemblyDataCollection* db = AssembliesByObjectType.Find(ObjectType);
	if (db == nullptr)
	{
		return ECraftingResult::Success;
	}
	for (auto& kvp : db->DataMap)
	{
		OutAssemblies.Add(kvp.Value);
	}
	return ECraftingResult::Success;
}

ECraftingResult FPresetManager::AddOrUpdateGraph2DRecord(FName Key, const FGraph2DRecord& Graph, FName& OutKey)
{
	static const FName defaultGraphBaseKey(TEXT("Graph2D"));
	Key = GetAvailableKey(defaultGraphBaseKey);
	return ECraftingResult::Success;
}

ECraftingResult FPresetManager::GetGraph2DRecord(const FName& Key, FGraph2DRecord& OutGraph) const
{
	if (GraphCollection.Contains(Key))
	{
		OutGraph = GraphCollection.FindChecked(Key);
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult FPresetManager::RemoveGraph2DRecord(const FName& Key)
{
	int32 numRemoved = GraphCollection.Remove(Key);
	return (numRemoved > 0) ? ECraftingResult::Success : ECraftingResult::Error;
}

ECraftingResult FPresetManager::RemoveProjectAssemblyForPreset(const FName& PresetID)
{
	FBIMAssemblySpec assembly;
	EObjectType objectType = CraftingNodePresets.GetPresetObjectType(PresetID);
	if (TryGetProjectAssemblyForPreset(objectType, PresetID, assembly))
	{
		FAssemblyDataCollection* db = AssembliesByObjectType.Find(objectType);
		if (ensureAlways(db != nullptr))
		{
			db->RemoveData(assembly);
			return ECraftingResult::Success;
		}
	}
	return ECraftingResult::Error;
}

ECraftingResult FPresetManager::UpdateProjectAssembly(const FBIMAssemblySpec& Assembly)
{
	FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(Assembly.ObjectType);
	db.AddData(Assembly);
	return ECraftingResult::Success;
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

bool FPresetManager::TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FName& PresetID, FBIMAssemblySpec& OutAssembly) const
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
		presetAssembly = db->GetData(TEXT("default"));
	}

	if (presetAssembly != nullptr)
	{
		OutAssembly = *presetAssembly;
		return true;
	}
	return false;
}

/*
Starting point: get a tree
*/

	const FBIMAssemblySpec *FPresetManager::GetAssemblyByKey(EToolMode ToolMode, const FName &Key) const
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



