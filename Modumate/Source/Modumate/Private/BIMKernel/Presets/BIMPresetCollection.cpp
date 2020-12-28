// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMCSVReader.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ModumateCore/ModumateScriptProcessor.h"


/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
Note: we don't empty the container because the function is recursive
*/
EBIMResult FBIMPresetCollection::GetDependentPresets(const FGuid& PresetID, TArray<FGuid>& OutPresets) const
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

EBIMResult FBIMPresetCollection::GetPropertyFormForPreset(const FGuid& PresetID, TMap<FString, FBIMNameType> &OutForm) const
{
	const FBIMPresetInstance* preset = PresetFromGUID(PresetID);
	if (preset == nullptr)
	{
		return EBIMResult::Error;
	}

	OutForm = preset->FormItemToProperty;
	return EBIMResult::Success;
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
		else
		{
			NodeDescriptors.Add(tableData.NodeType.TypeName, tableData.NodeType);
		}

		ensureAlways(tableData.ProcessTagPathRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
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

	// If we're reading an old collection, build the GUID map
	if (PresetsByGUID.Num() == 0)
	{
		for (auto& kvp : Presets_DEPRECATED)
		{
			if (kvp.Value.GUID.IsValid())
			{
				AddPreset(kvp.Value);
			}
		}
	}

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


EBIMResult FBIMPresetCollection::CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FGuid& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec)
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
	GetDependentPresets(LayerPresetKey, outpresetKeys);
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
	int32 sequence = 1;
	while (Presets_DEPRECATED.Find(OutKey) != nullptr)
	{
		OutKey = FBIMKey(FString::Printf(TEXT("%s-%d"), *returnKey, sequence++));
	}
	return EBIMResult::Success;
}

bool FBIMPresetCollection::Matches(const FBIMPresetCollection& OtherCollection) const
{
	if (NodeDescriptors.Num() != OtherCollection.NodeDescriptors.Num())
	{
		return false;
	}

	for (const auto& kvp : OtherCollection.NodeDescriptors)
	{
		const FBIMPresetTypeDefinition* typeDef = NodeDescriptors.Find(kvp.Key);
		if (typeDef == nullptr)
		{
			return false;
		}
		if (!typeDef->Matches(kvp.Value))
		{
			return false;
		}
	}

	if (PresetsByGUID.Num() != OtherCollection.PresetsByGUID.Num())
	{
		return false;
	}

	for (const auto& kvp : OtherCollection.PresetsByGUID)
	{
		const FBIMPresetInstance* preset = PresetFromGUID(kvp.Key);
		if (preset == nullptr)
		{
			return false;
		}
		if (!preset->Matches(kvp.Value))
		{
			return false;
		}
	}

	return true;
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
