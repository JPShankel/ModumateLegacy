// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMCSVReader.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ModumateCore/ModumateScriptProcessor.h"


/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
Note: we don't empty the container because the function is recursive
*/
EBIMResult FBIMPresetCollection::GetDependentPresets(const FBIMKey& PresetID, TArray<FBIMKey>& OutPresets) const
{
	TArray<FBIMKey> presetStack;
	presetStack.Push(PresetID);
	while (presetStack.Num() > 0)
	{
		FBIMKey presetID = presetStack.Pop();
		const FBIMPresetInstance* preset = Presets.Find(presetID);
		if (!ensureAlways(preset != nullptr))
		{
			return EBIMResult::Error;
		}

		for (auto &childNode : preset->ChildPresets)
		{
			OutPresets.AddUnique(childNode.PresetID);
			presetStack.Push(childNode.PresetID);
		}
		if (!preset->SlotConfigPresetID.IsNone())
		{
			OutPresets.AddUnique(preset->SlotConfigPresetID);
			presetStack.Push(preset->SlotConfigPresetID);
		}
		for (auto& part : preset->PartSlots)
		{
			OutPresets.AddUnique(part.PartPreset);
			presetStack.Push(part.PartPreset);
		}
	}
	return EBIMResult::Success;
}

EObjectType FBIMPresetCollection::GetPresetObjectType(const FBIMKey& PresetID) const
{
	const FBIMPresetInstance* preset = Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return EObjectType::OTNone;
	}
	return preset->ObjectType;
}

EBIMResult FBIMPresetCollection::GetPropertyFormForPreset(const FBIMKey& PresetID, TMap<FString, FBIMNameType> &OutForm) const
{
	const FBIMPresetInstance* preset = Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return EBIMResult::Error;
	}

	const FBIMPresetTypeDefinition* descriptor = NodeDescriptors.Find(preset->NodeType);
	if (descriptor == nullptr)
	{
		return EBIMResult::Error;
	}
	OutForm = descriptor->FormItemToProperty;
	return EBIMResult::Success;
}

// TODO: Loading data from a CSV manifest file is not going to be required long term. 
// Ultimately we will develop a compiler from this code that generates a record that can be read more efficiently
// Once this compiler is in the shape we intend, we will determine where in the toolchain this code will reside we can refactor for long term sustainability
// Until then this is a prototypical development space used to prototype the relational database structure being authored in Excel
EBIMResult FBIMPresetCollection::LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages)
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
		ensureAlways(tableData.ProcessPresetRow(Row, RowNumber, Presets, OutStarters, OutMessages) == EBIMResult::Success);
	});

	processor.AddRule(kInputPin, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessInputPinRow(Row, RowNumber, OutMessages) == EBIMResult::Success);
	});

	TArray<FString> fileList;
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

			if (!processor.ParseFile(*(ManifestPath / file)))
			{
				OutMessages.Add(FString::Printf(TEXT("Failed to load CSV file %s"), *file));
				return EBIMResult::Error;
			}

			// Make sure the last preset we were reading ends up in the map
			if (!tableData.Preset.PresetID.IsNone())
			{
				Presets.Add(tableData.Preset.PresetID, tableData.Preset);
			}
		}
		return EBIMResult::Success;
	}
	OutMessages.Add(FString::Printf(TEXT("Failed to load manifest file %s"), *ManifestFile));
	return EBIMResult::Error;
}

EBIMResult FBIMPresetCollection::CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FBIMKey& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec)
{
	FBIMPresetCollection previewCollection;

	// Build a temporary top-level assembly preset to host the single layer
	FBIMPresetInstance assemblyPreset;
	assemblyPreset.PresetID = FBIMKey(TEXT("TempIconPreset"));
	assemblyPreset.NodeScope = EBIMValueScope::Assembly;

	// Give the temporary assembly a single layer child
	FBIMPresetPinAttachment &attachment = assemblyPreset.ChildPresets.AddDefaulted_GetRef();
	attachment.ParentPinSetIndex = 0;
	attachment.ParentPinSetPosition = 0;
	attachment.PresetID = LayerPresetKey;
	assemblyPreset.ObjectType = ObjectType;

	// Add the temp assembly and layer presets
	previewCollection.Presets.Add(assemblyPreset.PresetID, assemblyPreset);

	const FBIMPresetInstance* layerPreset = Presets.Find(LayerPresetKey);
	if (ensureAlways(layerPreset != nullptr))
	{
		previewCollection.Presets.Add(layerPreset->PresetID, *layerPreset);
	}
	else
	{
		return EBIMResult::Error;
	}

	// Add presets from dependents
	TArray<FBIMKey> outpresetKeys;
	GetDependentPresets(LayerPresetKey, outpresetKeys);
	for (auto& curPreset : outpresetKeys)
	{
		const FBIMPresetInstance* original = Presets.Find(curPreset);
		if (ensureAlways(original != nullptr))
		{
			previewCollection.Presets.Add(original->PresetID, *original);
		}
	}

	return OutAssemblySpec.FromPreset(InDB, previewCollection, assemblyPreset.PresetID);
}
