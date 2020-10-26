// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "BIMKernel/BIMCSVReader.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateScriptProcessor.h"


bool FBIMPreset::FChildAttachment::operator==(const FBIMPreset::FChildAttachment &OtherAttachment) const
{
	return ParentPinSetIndex == OtherAttachment.ParentPinSetIndex &&
		ParentPinSetPosition == OtherAttachment.ParentPinSetPosition &&
		PresetID == OtherAttachment.PresetID;
}

ECraftingResult FBIMPresetCollection::ToDataRecords(TArray<FCraftingPresetRecord> &OutRecords) const
{
	for (auto &kvp : Presets)
	{
		FCraftingPresetRecord &presetRec = OutRecords.AddDefaulted_GetRef();
		kvp.Value.ToDataRecord(presetRec);
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMPresetCollection::FromDataRecords(const TArray<FCraftingPresetRecord> &Records)
{
	Presets.Empty();

	for (auto &presetRecord : Records)
	{
		FBIMPreset newPreset;
		if (newPreset.FromDataRecord(*this, presetRecord) == ECraftingResult::Success)
		{
			Presets.Add(newPreset.PresetID, newPreset);
		}
	}

	return ECraftingResult::Success;
}

bool FBIMPreset::Matches(const FBIMPreset &OtherPreset) const
{
	if (NodeType != OtherPreset.NodeType)
	{
		return false;
	}

	if (PresetID != OtherPreset.PresetID)
	{
		return false;
	}

	if (ChildPresets.Num() != OtherPreset.ChildPresets.Num())
	{
		return false;
	}

	for (auto &cp : ChildPresets)
	{
		if (!OtherPreset.ChildPresets.Contains(cp))
		{
			return false;
		}
	}

	if (ParentTagPaths.Num() != OtherPreset.ParentTagPaths.Num())
	{
		return false;
	}

	for (auto &ptp : ParentTagPaths)
	{
		if (!OtherPreset.ParentTagPaths.Contains(ptp))
		{
			return false;
		}
	}

	if (MyTagPath != OtherPreset.MyTagPath)
	{
		return false;
	}

	if (!Properties.Matches(OtherPreset.Properties))
	{
		return false;
	}

	return true;
}

ECraftingResult FBIMPreset::ToDataRecord(FCraftingPresetRecord& OutRecord) const
{
	OutRecord.DisplayName = GetDisplayName();
	OutRecord.NodeType = NodeType;
	OutRecord.PresetID = PresetID;
	OutRecord.SlotConfigPresetID = SlotConfigPresetID;
	OutRecord.CategoryTitle = CategoryTitle.ToString();
	OutRecord.ObjectType = ObjectType;
	MyTagPath.ToString(OutRecord.MyTagPath);

	for (auto& ptp : ParentTagPaths)
	{
		ptp.ToString(OutRecord.ParentTagPaths.AddDefaulted_GetRef());
	}

	for (auto& childPreset : ChildPresets)
	{
		OutRecord.ChildSetIndices.Add(childPreset.ParentPinSetIndex);
		OutRecord.ChildSetPositions.Add(childPreset.ParentPinSetPosition);
		OutRecord.ChildPresets.Add(childPreset.PresetID);
	}

	for (auto& partSlot : PartSlots)
	{
		OutRecord.PartPresets.Add(partSlot.PartPreset);
		OutRecord.PartIDs.Add(partSlot.ID);
		OutRecord.PartParentIDs.Add(partSlot.ParentID);
		OutRecord.PartSlotNames.Add(partSlot.SlotName);
	}

	Properties.ToDataRecord(OutRecord.PropertyRecord);

	return ECraftingResult::Success;
}

ECraftingResult FBIMPreset::SortChildNodes()
{
	ChildPresets.Sort([](const FBIMPreset::FChildAttachment &lhs, const FBIMPreset::FChildAttachment &rhs)
	{
		if (lhs.ParentPinSetIndex < rhs.ParentPinSetIndex)
		{
			return true;
		}
		if (lhs.ParentPinSetIndex > rhs.ParentPinSetIndex)
		{
			return false;
		}
		return lhs.ParentPinSetPosition < rhs.ParentPinSetPosition;
	});
	return ECraftingResult::Success;
}

ECraftingResult FBIMPreset::FromDataRecord(const FBIMPresetCollection &PresetCollection, const FCraftingPresetRecord &Record)
{
	NodeType = Record.NodeType;
	CategoryTitle = FText::FromString(Record.CategoryTitle);

	const FBIMPresetNodeType *nodeType = PresetCollection.NodeDescriptors.Find(NodeType);
	// TODO: this ensure will fire if expected presets have become obsolete, resave to fix
	if (!ensureAlways(nodeType != nullptr))
	{
		return ECraftingResult::Error;
	}

	PresetID = Record.PresetID;
	SlotConfigPresetID = Record.SlotConfigPresetID;
	ObjectType = Record.ObjectType;
	NodeScope = nodeType->Scope;

	Properties.Empty();
	ChildPresets.Empty();

	Properties.FromDataRecord(Record.PropertyRecord);

	if (ensureAlways(Record.ChildSetIndices.Num() == Record.ChildPresets.Num() &&
		Record.ChildSetPositions.Num() == Record.ChildPresets.Num()))
	{
		for (int32 i = 0; i < Record.ChildPresets.Num(); ++i)
		{
			FBIMPreset::FChildAttachment &attachment = ChildPresets.AddDefaulted_GetRef();
			attachment.ParentPinSetIndex = Record.ChildSetIndices[i];
			attachment.ParentPinSetPosition = Record.ChildSetPositions[i];
			attachment.PresetID = Record.ChildPresets[i];
		}
	}

	if (ensureAlways(Record.PartIDs.Num() == Record.PartParentIDs.Num() &&
		Record.PartIDs.Num() == Record.PartPresets.Num() &&
		Record.PartIDs.Num() == Record.PartSlotNames.Num()))
	{
		for (int32 i = 0; i < Record.PartIDs.Num();++i)
		{
			FPartSlot &partSlot = PartSlots.AddDefaulted_GetRef();
			partSlot.ID = Record.PartIDs[i];
			partSlot.ParentID = Record.PartParentIDs[i];
			partSlot.PartPreset = Record.PartPresets[i];
			partSlot.SlotName = Record.PartSlotNames[i];
		}
	}

	MyTagPath.FromString(Record.MyTagPath);

	for (auto &ptp : Record.ParentTagPaths)
	{
		ParentTagPaths.AddDefaulted_GetRef().FromString(ptp);
	}

	FString displayName;
	if (TryGetProperty(BIMPropertyNames::Name, displayName))
	{
		DisplayName = FText::FromString(displayName);
	}

	return ECraftingResult::Success;
}

/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
Note: we don't empty the container because the function is recursive
*/
ECraftingResult FBIMPresetCollection::GetDependentPresets(const FBIMKey& PresetID, TArray<FBIMKey>& OutPresets) const
{
	TArray<FBIMKey> presetStack;
	presetStack.Push(PresetID);
	while (presetStack.Num() > 0)
	{
		FBIMKey presetID = presetStack.Pop();
		const FBIMPreset *preset = Presets.Find(presetID);
		if (!ensureAlways(preset != nullptr))
		{
			return ECraftingResult::Error;
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
	return ECraftingResult::Success;
}

FString FBIMPreset::GetDisplayName() const
{
	return Properties.GetProperty(EBIMValueScope::Preset, BIMPropertyNames::Name);
}

bool FBIMPreset::HasProperty(const FBIMNameType& Name) const
{
	return Properties.HasProperty(NodeScope, Name);
}

Modumate::FModumateCommandParameter FBIMPreset::GetProperty(const FBIMNameType& Name) const
{
	return Properties.GetProperty(NodeScope, Name);
}

Modumate::FModumateCommandParameter FBIMPreset::GetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name) const
{
	return Properties.GetProperty(Scope, Name);
}

void FBIMPreset::SetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name, const Modumate::FModumateCommandParameter& V)
{
	Properties.SetProperty(Scope, Name, V);
}

void FBIMPreset::SetProperties(const FBIMPropertySheet& InProperties)
{
	Properties = InProperties;
}

bool FBIMPreset::SupportsChild(const FBIMPreset& CandidateChild) const
{
	for (auto& childPath : CandidateChild.ParentTagPaths)
	{
		if (childPath.MatchesPartial(MyTagPath))
		{
			return true;
		}
	}
	return false;
}

EObjectType FBIMPresetCollection::GetPresetObjectType(const FBIMKey& PresetID) const
{
	const FBIMPreset *preset = Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return EObjectType::OTNone;
	}
	return preset->ObjectType;
}

ECraftingResult FBIMPresetCollection::GetPropertyFormForPreset(const FBIMKey& PresetID, TMap<FString, FBIMNameType> &OutForm) const
{
	const FBIMPreset* preset = Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return ECraftingResult::Error;
	}

	const FBIMPresetNodeType* descriptor = NodeDescriptors.Find(preset->NodeType);
	if (descriptor == nullptr)
	{
		return ECraftingResult::Error;
	}
	OutForm = descriptor->FormItemToProperty;
	return ECraftingResult::Success;
}

// TODO: Loading data from a CSV manifest file is not going to be required long term. 
// Ultimately we will develop a compiler from this code that generates a record that can be read more efficiently
// Once this compiler is in the shape we intend, we will determine where in the toolchain this code will reside we can refactor for long term sustainability
// Until then this is a prototypical development space used to prototype the relational database structure being authored in Excel
ECraftingResult FBIMPresetCollection::LoadCSVManifest(const FString& ManifestPath, const FString& ManifestFile, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages)
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
		ensureAlways(tableData.ProcessNodeTypeRow(Row, RowNumber, OutMessages)==ECraftingResult::Success);
	});

	processor.AddRule(kProperty, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessPropertyDeclarationRow(Row, RowNumber, OutMessages) == ECraftingResult::Success);
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

		ensureAlways(tableData.ProcessTagPathRow(Row, RowNumber, OutMessages) == ECraftingResult::Success);
	});

	processor.AddRule(kPreset, [&OutMessages, &OutStarters, &tableData, &ManifestFile, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessPresetRow(Row, RowNumber, Presets, OutStarters, OutMessages)==ECraftingResult::Success);
	});

	processor.AddRule(kInputPin, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		ensureAlways(tableData.ProcessInputPinRow(Row, RowNumber, OutMessages) == ECraftingResult::Success);
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
				return ECraftingResult::Error;
			}

			// Make sure the last preset we were reading ends up in the map
			if (!tableData.Preset.PresetID.IsNone())
			{
				Presets.Add(tableData.Preset.PresetID, tableData.Preset);
			}

			if (tableData.UPropertySheet != nullptr)
			{
				tableData.UPropertySheet->RemoveFromRoot();
				tableData.UPropertySheet = nullptr;
			}

		}
		return ECraftingResult::Success;
	}
	OutMessages.Add(FString::Printf(TEXT("Failed to load manifest file %s"), *ManifestFile));
	return ECraftingResult::Error;
}

ECraftingResult FBIMPresetCollection::CreateAssemblyFromLayerPreset(const FModumateDatabase& InDB, const FBIMKey& LayerPresetKey, EObjectType ObjectType, FBIMAssemblySpec& OutAssemblySpec)
{
	FBIMPresetCollection previewCollection;

	// Build a temporary top-level assembly preset to host the single layer
	FBIMPreset assemblyPreset;
	assemblyPreset.PresetID = FBIMKey(TEXT("TempIconPreset"));
	assemblyPreset.NodeScope = EBIMValueScope::Assembly;

	// Give the temporary assembly a single layer child
	FBIMPreset::FChildAttachment &attachment = assemblyPreset.ChildPresets.AddDefaulted_GetRef();
	attachment.ParentPinSetIndex = 0;
	attachment.ParentPinSetPosition = 0;
	attachment.PresetID = LayerPresetKey;
	assemblyPreset.ObjectType = ObjectType;

	// Add the temp assembly and layer presets
	previewCollection.Presets.Add(assemblyPreset.PresetID, assemblyPreset);

	const FBIMPreset* layerPreset = Presets.Find(LayerPresetKey);
	if (ensureAlways(layerPreset != nullptr))
	{
		previewCollection.Presets.Add(layerPreset->PresetID, *layerPreset);
	}
	else
	{
		return ECraftingResult::Error;
	}

	// Add presets from dependents
	TArray<FBIMKey> outpresetKeys;
	GetDependentPresets(LayerPresetKey, outpresetKeys);
	for (auto& curPreset : outpresetKeys)
	{
		const FBIMPreset* original = Presets.Find(curPreset);
		if (ensureAlways(original != nullptr))
		{
			previewCollection.Presets.Add(original->PresetID, *original);
		}
	}

	return OutAssemblySpec.FromPreset(InDB, previewCollection, assemblyPreset.PresetID);
}
