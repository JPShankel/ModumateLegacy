// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMCSVReader.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateScriptProcessor.h"

/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
*/
EBIMResult FBIMPresetCollection::GetAllDescendentPresets(const FGuid& PresetID, TArray<FGuid>& OutPresets) const
{
	TArray<FGuid> presetStack;
	presetStack.Push(PresetID);
	while (presetStack.Num() > 0)
	{
		FGuid presetID = presetStack.Pop();
		const FBIMPresetInstance* preset = PresetFromGUID(presetID);
		if (!ensureAlways(preset != nullptr))
		{
			return EBIMResult::Error;
		}

		for (auto &childNode : preset->ChildPresets)
		{
			OutPresets.AddUnique(childNode.PresetGUID);
			presetStack.Push(childNode.PresetGUID);
		}
		if (preset->SlotConfigPresetGUID.IsValid())
		{
			OutPresets.AddUnique(preset->SlotConfigPresetGUID);
			presetStack.Push(preset->SlotConfigPresetGUID);
		}
		for (auto& part : preset->PartSlots)
		{
			if (part.PartPresetGUID.IsValid())
			{
				OutPresets.AddUnique(part.PartPresetGUID);
				presetStack.Push(part.PartPresetGUID);
			}
		}

		FGuid meshAsset;
		if (preset->Properties.TryGetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, meshAsset))
		{
			OutPresets.AddUnique(meshAsset);
		}
	}
	return EBIMResult::Success;
}

/*
* Given a PresetID, get all other presets that depend on it
*/
EBIMResult FBIMPresetCollection::GetAllAncestorPresets(const FGuid& PresetGUID, TArray<FGuid>& OutPresets) const
{
	for (auto& preset : PresetsByGUID)
	{
		TArray<FGuid> descendents;
		GetAllDescendentPresets(preset.Key, descendents);
		if (descendents.Contains(PresetGUID))
		{
			OutPresets.Add(preset.Key);
		}
	}
	return EBIMResult::Success;
}

EObjectType FBIMPresetCollection::GetPresetObjectType(const FGuid& PresetID) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(PresetID);
	if (preset == nullptr)
	{
		return EObjectType::OTNone;
	}
	return preset->ObjectType;
}

EBIMResult FBIMPresetCollection::GetPresetsByPredicate(const TFunction<bool(const FBIMPresetInstance& Preset)>& Predicate, TArray<FGuid>& OutPresets) const
{
	for (auto& kvp : PresetsByGUID)
	{
		if (Predicate(kvp.Value))
		{
			OutPresets.Add(kvp.Value.GUID);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetPresetsForSlot(const FGuid& SlotPresetGUID, TArray<FGuid>& OutPresets) const
{
	const FBIMPresetInstance* slotPreset = PresetFromGUID(SlotPresetGUID);
	if (slotPreset == nullptr)
	{
		return EBIMResult::Error;
	}

	FString slotPathNCP;
	if (slotPreset->Properties.TryGetProperty(EBIMValueScope::Slot, BIMPropertyNames::SupportedNCPs, slotPathNCP))
	{
		FBIMTagPath tagPath;
		EBIMResult res = tagPath.FromString(slotPathNCP);
		if (res == EBIMResult::Success)
		{
			return GetPresetsByPredicate([tagPath](const FBIMPresetInstance& Preset)
			{
				return Preset.MyTagPath.MatchesExact(tagPath);
			}, OutPresets);
		}
		else
		{
			return res;
		}
	}
	return EBIMResult::Error;
}


// TODO: Loading data from a CSV manifest file is not going to be required long term. 
// Ultimately we will develop a compiler from this code that generates a record that can be read more efficiently
// Once this compiler is in the shape we intend, we will determine where in the toolchain this code will reside we can refactor for long term sustainability
// Until then this is a prototypical development space used to prototype the relational database structure being authored in Excel
EBIMResult FBIMPresetCollection::LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FGuid>& OutStarters, TArray<FString>& OutMessages)
{
	FModumateCSVScriptProcessor processor;

	static const FName kTypeName = TEXT("TYPENAME");
	static const FName kProperty = TEXT("PROPERTY");
	static const FName kTagPaths = TEXT("TAGPATHS");
	static const FName kPreset = TEXT("PRESET");
	static const FName kInputPin = TEXT("INPUTPIN");
	static const FName kString = TEXT("String");
	static const FName kDimension = TEXT("Dimension");

	FBIMCSVReader tableData;
	tableData.KeyGuidMap.Empty();
	tableData.GuidKeyMap.Empty();

	processor.AddRule(kTypeName, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessNodeTypeRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kProperty, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessPropertyDeclarationRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kTagPaths, [&OutMessages, &tableData, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		if (tableData.NodeType.TypeName.IsNone())
		{
			OutMessages.Add(FString::Printf(TEXT("No node definition")));
		}

		// tableData.NodeType may be updated by the tag path row, which contains matrices that can add to the form template
		if (ensureAlways(tableData.ProcessTagPathRow(Row, RowNumber, OutMessages) == EBIMResult::Success))
		{
			NodeDescriptors.Add(tableData.NodeType.TypeName, tableData.NodeType);
		}
	});

	processor.AddRule(kPreset, [&OutMessages, &OutStarters, &tableData, &ManifestFile, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessPresetRow(Row, RowNumber, *this, OutStarters, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kInputPin, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessInputPinRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	TArray<FString> fileList;
	TMap<FBIMKey, FGuid> keyGUIDMap;
	if (FFileHelper::LoadFileToStringArray(fileList, *(ManifestPath / ManifestFile)))
	{
		for (auto &file : fileList)
		{
			// add a blank line in manifest to halt processing
			if (file.IsEmpty())
			{
				break;
			}

			if (file[0] == TCHAR(';'))
			{
				continue;
			}

			tableData = FBIMCSVReader();
			tableData.CurrentFile = file;

			if (!processor.ParseFile(*(ManifestPath / file)))
			{
				OutMessages.Add(FString::Printf(TEXT("Failed to load CSV file %s"), *file));
				return EBIMResult::Error;
			}

			// Make sure the last preset we were reading ends up in the map
			if (tableData.Preset.GUID.IsValid())
			{
				AddPreset(tableData.Preset);
				FBIMCSVReader::KeyGuidMap.Add(tableData.Preset.PresetID, tableData.Preset.GUID);
				FBIMCSVReader::GuidKeyMap.Add(tableData.Preset.GUID, tableData.Preset.PresetID);
			}
		}
		PostLoad();
		return EBIMResult::Success;
	}
	OutMessages.Add(FString::Printf(TEXT("Failed to load manifest file %s"), *ManifestFile));
	return EBIMResult::Error;
}

EBIMResult FBIMPresetCollection::PostLoad()
{
	UsedGUIDs.Empty();
	AllNCPs.Empty();

	for (auto& kvp : PresetsByGUID)
	{
		UsedGUIDs.Add(kvp.Value.GUID);
		AllNCPs.Add(kvp.Value.MyTagPath);
		
		FBIMPresetTypeDefinition* typeDef = NodeDescriptors.Find(kvp.Value.NodeType);
		if (ensureAlways(typeDef != nullptr))
		{
			kvp.Value.TypeDefinition = *typeDef;
		}
	}

	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetAvailableGUID(FGuid& OutGUID)
{
	int32 sanity = 0;
	static constexpr int32 sanityCheck = 1000;

	do {
		OutGUID = FGuid::NewGuid();
	} while (UsedGUIDs.Contains(OutGUID) && ++sanity < sanityCheck);

	if (sanity == sanityCheck)
	{
		return EBIMResult::Error;
	}
	UsedGUIDs.Add(OutGUID);
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FGuid& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec) const
{
	FBIMPresetCollection previewCollection;

	// Build a temporary top-level assembly preset to host the single layer
	FBIMPresetInstance assemblyPreset;
	assemblyPreset.PresetID = FBIMKey(TEXT("TempIconPreset"));
	assemblyPreset.GUID = FGuid::NewGuid();
	assemblyPreset.NodeScope = EBIMValueScope::Assembly;

	// Give the temporary assembly a single layer child
	FBIMPresetPinAttachment &attachment = assemblyPreset.ChildPresets.AddDefaulted_GetRef();
	attachment.ParentPinSetIndex = 0;
	attachment.ParentPinSetPosition = 0;
	attachment.PresetGUID = LayerPresetKey;
	assemblyPreset.ObjectType = ObjectType;

	// Add the temp assembly and layer presets
	previewCollection.AddPreset(assemblyPreset);

	const FBIMPresetInstance* layerPreset = PresetFromGUID(LayerPresetKey);
	if (ensureAlways(layerPreset != nullptr))
	{
		previewCollection.AddPreset(*layerPreset);
	}
	else
	{
		return EBIMResult::Error;
	}

	// Add presets from dependents
	TArray<FGuid> outpresetKeys;
	GetAllDescendentPresets(LayerPresetKey, outpresetKeys);
	for (auto& curPreset : outpresetKeys)
	{
		const FBIMPresetInstance* original = PresetFromGUID(curPreset);
		if (ensureAlways(original != nullptr))
		{
			previewCollection.AddPreset(*original);
		}
	}

	return OutAssemblySpec.FromPreset(InDB, previewCollection, assemblyPreset.GUID);
}

EBIMResult FBIMPresetCollection::GenerateBIMKeyForPreset(const FGuid& PresetID, FBIMKey& OutKey) const
{
	OutKey = FBIMKey();

	TArray<FGuid> idStack;
	idStack.Push(PresetID);

	FString returnKey;

	while (idStack.Num() > 0)
	{
		FGuid id = idStack.Pop();
		const FBIMPresetInstance* preset = PresetFromGUID(id);
		if (!ensureAlways(preset != nullptr))
		{
			return EBIMResult::Error;
		}

		FString key;
		preset->MyTagPath.ToString(key);
		returnKey.Append(key);

		FString name;
		if (preset->TryGetProperty(BIMPropertyNames::Name, name))
		{
			returnKey.Append(name);
		}

		for (auto& childPreset : preset->ChildPresets)
		{
			idStack.Push(childPreset.PresetGUID);
		}

		for (auto& part : preset->PartSlots)
		{
			if (!part.PartPreset.IsNone())
			{
				idStack.Push(part.PartPresetGUID);
			}
		}
	}

	if (returnKey.IsEmpty())
	{
		return EBIMResult::Error;
	}

	returnKey.RemoveSpacesInline();

	OutKey = FBIMKey(returnKey);

	return EBIMResult::Success;
}

bool FBIMPresetCollection::operator==(const FBIMPresetCollection& RHS) const
{
	if (NodeDescriptors.Num() != RHS.NodeDescriptors.Num())
	{
		return false;
	}

	for (const auto& kvp : RHS.NodeDescriptors)
	{
		const FBIMPresetTypeDefinition* typeDef = NodeDescriptors.Find(kvp.Key);
		if (typeDef == nullptr || *typeDef != kvp.Value)
		{
			return false;
		}
	}

	if (PresetsByGUID.Num() != RHS.PresetsByGUID.Num())
	{
		return false;
	}

	for (const auto& kvp : RHS.PresetsByGUID)
	{
		const FBIMPresetInstance* preset = PresetFromGUID(kvp.Key);
		if (preset == nullptr || *preset != kvp.Value)
		{
			return false;
		}
	}

	return true;
}

bool FBIMPresetCollection::operator!=(const FBIMPresetCollection& RHS) const
{
	return !(*this == RHS);
}

EBIMResult FBIMPresetCollection::GetNCPForPreset(const FGuid& InPresetGUID, FBIMTagPath& OutNCP) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(InPresetGUID);
	if (preset == nullptr)
	{
		return EBIMResult::Error;
	}
	OutNCP = preset->MyTagPath;
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetPresetsForNCP(const FBIMTagPath& InNCP, TArray<FGuid>& OutPresets) const
{
	return GetPresetsByPredicate([InNCP](const FBIMPresetInstance& Preset) 
	{
		return Preset.MyTagPath.Tags.Num() >= InNCP.Tags.Num() && Preset.MyTagPath.MatchesPartial(InNCP);
	},OutPresets);
}

EBIMResult FBIMPresetCollection::GetNCPSubcategories(const FBIMTagPath& InNCP, TArray<FString>& OutSubcats) const
{
	for (auto& ncp : AllNCPs)
	{
		if (ncp.Tags.Num() > InNCP.Tags.Num() && ncp.MatchesPartial(InNCP))
		{
			OutSubcats.AddUnique(ncp.Tags[InNCP.Tags.Num()]);
		}
	}
	return EBIMResult::Success;
}

FBIMPresetInstance* FBIMPresetCollection::PresetFromGUID(const FGuid& InGUID)
{
	return PresetsByGUID.Find(InGUID);
}

const FBIMPresetInstance* FBIMPresetCollection::PresetFromGUID(const FGuid& InGUID) const
{
	return PresetsByGUID.Find(InGUID);
}

EBIMResult FBIMPresetCollection::AddPreset(const FBIMPresetInstance& InPreset)
{
	if (ensureAlways(InPreset.GUID.IsValid()))
	{
		PresetsByGUID.Add(InPreset.GUID, InPreset);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetCollection::RemovePreset(const FGuid& InGUID)
{
	FBIMPresetInstance* preset = PresetFromGUID(InGUID);
	if (preset != nullptr)
	{
		PresetsByGUID.Remove(preset->GUID);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

TSharedPtr<FBIMPresetDelta> FBIMPresetCollection::MakeUpdateDelta(FBIMPresetInstance& UpdatedPreset) const
{
	TSharedPtr<FBIMPresetDelta> presetDelta = MakeShared<FBIMPresetDelta>();

	presetDelta->NewState = UpdatedPreset;
	const FBIMPresetInstance* oldPreset = PresetsByGUID.Find(UpdatedPreset.GUID);
	if (oldPreset != nullptr)
	{
		presetDelta->OldState = *oldPreset;
	}

	return presetDelta;
}

TSharedPtr<FBIMPresetDelta> FBIMPresetCollection::MakeCreateNewDelta(FBIMPresetInstance& NewPreset)
{
	if (ensureAlways(!NewPreset.GUID.IsValid()))
	{
		GetAvailableGUID(NewPreset.GUID);
		return MakeUpdateDelta(NewPreset);
	}
	return nullptr;
}

EBIMResult FBIMPresetCollection::ForEachPreset(const TFunction<void(const FBIMPresetInstance& Preset)>& Operation) const
{
	for (auto& kvp : PresetsByGUID)
	{
		Operation(kvp.Value);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetAvailablePresetsForSwap(const FGuid& ParentPresetID, const FGuid& PresetIDToSwap, TArray<FGuid>& OutAvailablePresets) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(PresetIDToSwap);
	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	return GetPresetsByPredicate([preset](const FBIMPresetInstance& Preset)
	{
		return Preset.MyTagPath.MatchesExact(preset->MyTagPath);
	}, OutAvailablePresets);

	return EBIMResult::Success;
}

EBIMResult FBIMPresetCollection::GetProjectAssembliesForObjectType(EObjectType ObjectType, TArray<FBIMAssemblySpec>& OutAssemblies) const
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

EBIMResult FBIMPresetCollection::RemoveProjectAssemblyForPreset(const FGuid& PresetID)
{
	FBIMAssemblySpec assembly;
	EObjectType objectType = GetPresetObjectType(PresetID);
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

EBIMResult FBIMPresetCollection::UpdateProjectAssembly(const FBIMAssemblySpec& Assembly)
{
	FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(Assembly.ObjectType);
	db.AddData(Assembly);
	return EBIMResult::Success;
}

bool FBIMPresetCollection::TryGetDefaultAssemblyForToolMode(EToolMode ToolMode, FBIMAssemblySpec& OutAssembly) const
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

bool FBIMPresetCollection::TryGetProjectAssemblyForPreset(EObjectType ObjectType, const FGuid& PresetID, FBIMAssemblySpec& OutAssembly) const
{
	const TModumateDataCollection<FBIMAssemblySpec>* db = AssembliesByObjectType.Find(ObjectType);

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

const FBIMAssemblySpec* FBIMPresetCollection::GetAssemblyByGUID(EToolMode ToolMode, const FGuid& Key) const
{
	EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(ToolMode);
	const FAssemblyDataCollection* db = AssembliesByObjectType.Find(objectType);
	if (db != nullptr)
	{
		return db->GetData(Key);
	}
	else
	{
		return nullptr;
	}
}

bool FBIMPresetCollection::ReadPresetsFromDocRecord(const FModumateDatabase& InDB, const FMOIDocumentRecord& DocRecord)
{
	*this = InDB.GetPresetCollection();

	for (auto& kvp : DocRecord.PresetCollection.NodeDescriptors)
	{
		if (!NodeDescriptors.Contains(kvp.Key))
		{
			NodeDescriptors.Add(kvp.Key, kvp.Value);
		}
	}

	PresetsByGUID.Append(DocRecord.PresetCollection.PresetsByGUID);

	// If any presets fail to build their assembly, remove them
	// They'll be replaced by the fallback system and will not be written out again
	TSet<FGuid> incompletePresets;
	for (auto& kvp : PresetsByGUID)
	{
		kvp.Value.CustomData.SaveCborFromJson();
		if (kvp.Value.ObjectType != EObjectType::OTNone)
		{
			FAssemblyDataCollection& db = AssembliesByObjectType.FindOrAdd(kvp.Value.ObjectType);
			FBIMAssemblySpec newSpec;
			if (newSpec.FromPreset(InDB, *this, kvp.Value.GUID) == EBIMResult::Success)
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
		RemovePreset(incompletePreset);
	}

	return PostLoad() == EBIMResult::Success;
}

bool FBIMPresetCollection::SavePresetsToDocRecord(FMOIDocumentRecord& DocRecord) const
{
	for (auto& kvp : PresetsByGUID)
	{
		// Only save presets that have been edited, the rest are in the shared db
		if (kvp.Value.Edited)
		{
			auto serializedPreset = kvp.Value;
			serializedPreset.CustomData.SaveJsonFromCbor();
			DocRecord.PresetCollection.AddPreset(serializedPreset);
			if (!DocRecord.PresetCollection.NodeDescriptors.Contains(kvp.Value.NodeType))
			{
				const FBIMPresetTypeDefinition* typeDef = NodeDescriptors.Find(kvp.Value.NodeType);
				if (ensureAlways(typeDef != nullptr))
				{
					DocRecord.PresetCollection.NodeDescriptors.Add(kvp.Value.NodeType, *typeDef);
				}
			};
		}
	}

	return true;
}

EBIMResult FBIMPresetCollection::GetBlankPresetForObjectType(EObjectType ObjectType, FBIMPresetInstance& OutPreset) const
{
	TArray<FGuid> guids;
	GetPresetsByPredicate([ObjectType](const FBIMPresetInstance& Preset) {return Preset.ObjectType == ObjectType; },guids);

	// TODO: ensure the first preset is a reasonable blank
	if (guids.Num() > 0)
	{
		const FBIMPresetInstance* preset = PresetFromGUID(guids[0]);
		if (preset != nullptr)
		{
			OutPreset = *preset;
			OutPreset.GUID.Invalidate();
			OutPreset.Edited = true;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}
