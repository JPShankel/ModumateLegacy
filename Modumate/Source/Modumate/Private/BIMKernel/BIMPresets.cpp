// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMNodeEditor.h"
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

	const FBIMPresetNodeType *nodeType = PresetCollection.NodeDescriptors.Find(NodeType);
	// TODO: this ensure will fire if expected presets have become obsolete, resave to fix
	if (!ensureAlways(nodeType != nullptr))
	{
		return ECraftingResult::Error;
	}

	PresetID = Record.PresetID;
	SlotConfigPresetID = Record.SlotConfigPresetID;

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

	return ECraftingResult::Success;
}

/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
Note: we don't empty the container because the function is recursive
*/
ECraftingResult FBIMPresetCollection::GetDependentPresets(const FBIMKey& PresetID, TSet<FBIMKey>& OutPresets) const
{
	const FBIMPreset *preset = Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return ECraftingResult::Error;
	}

	for (auto &childNode : preset->ChildPresets)
	{
		/*
		Only recurse if we haven't processed this ID yet
		Even if the same preset appears multiple times in a tree, its children will always be the same
		*/
		if (!OutPresets.Contains(childNode.PresetID))
		{
			OutPresets.Add(childNode.PresetID);
			GetDependentPresets(childNode.PresetID, OutPresets);
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
	static const FName kPrivate = TEXT("PRIVATE");

	struct FColumnRange
	{
		int32 first = -1;
		TArray<FString> columns;
		bool IsIn(int32 i)
		{
			return first >= 0 && i >= first && i < first + columns.Num();
		}

		const FString &Get(int32 i) const {
			return columns[i - first];
		}
	};

	struct FTableData
	{
		FBIMPresetNodeType nodeType;
		TArray<FBIMTagPath> myPaths;
		TArray<FBIMTagPath> parentPaths;

		FBIMPreset currentPreset;

		FColumnRange configRange, propertyRange, 
			myPathRange, parentPathRange, pinRange, 
			idRange, startInProjectRange,slotRange;
	};

	FTableData tableData;

	auto normalizeCell = [](const FString& Row)
	{
		FString cell;
		int32 sl = Row.Len();
		cell.Reserve(sl);
		for (int32 j = 0; j < sl; ++j)
		{
			TCHAR c = Row[j];
			if (!TChar<TCHAR>::IsWhitespace(c) && !TChar<TCHAR>::IsLinebreak(c))
			{
				cell.AppendChar(Row[j]);
			}
		}
		return cell;
	};

	processor.AddRule(kTypeName, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		tableData.nodeType.TypeName = Row[2];

		FName scope = Row[4];

		if (scope.IsNone())
		{
			tableData.nodeType.Scope = EBIMValueScope::Error;
		}
		else
		{
			tableData.nodeType.Scope = BIMValueScopeFromName(scope);
		}

		if (tableData.nodeType.Scope == EBIMValueScope::Error || tableData.nodeType.Scope == EBIMValueScope::None)
		{
			OutMessages.Add(FString::Printf(TEXT("Unscoped Table")));
		}
	});

	processor.AddRule(kProperty, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		FName propertyType;
		FString propertyName;
		FBIMPropertyValue propertyValue;

		for (int32 i = 1; i < Row.Num(); ++i)
		{
			FString rowStr = Row[i];
			if (!rowStr.IsEmpty())
			{
				if (propertyType.IsNone())
				{
					propertyType = *rowStr;
				}
				else if (propertyName.IsEmpty())
				{
					propertyName = rowStr;
				}
				else
				{
					propertyValue = FBIMPropertyValue(Row[i]);
					break;
				}
			}
		}

		if (propertyValue.Scope == EBIMValueScope::None)
		{
			OutMessages.Add(FString::Printf(TEXT("Bad property")));
		}
		else
		{
			tableData.nodeType.Properties.SetProperty(propertyValue.Scope, propertyValue.Name, FBIMPropertyValue::FValue());
			if (propertyType != kPrivate)
			{
				tableData.nodeType.FormItemToProperty.Add(propertyName, propertyValue.QN());
			}
		}
	});

	processor.AddRule(kTagPaths, [&OutMessages, normalizeCell, &tableData, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		if (tableData.nodeType.TypeName.IsNone())
		{
			OutMessages.Add(FString::Printf(TEXT("No node definition")));
		}
		else
		{
			NodeDescriptors.Add(tableData.nodeType.TypeName, tableData.nodeType);
		}

		enum State {
			Configuration,
			Properties,
			InputPins,
			ParentPaths,
			ID,
			StartsInProject,
			MyPath,
			Slots
		};

		State state = Configuration;

		tableData.configRange.first = 1;

		for (int32 i = 1; i < Row.Num(); ++i)
		{
			FString cell = Row[i];
			TArray<FString> dataType;

			if (cell.ParseIntoArray(dataType, TEXT("=")) && dataType[0].Equals(TEXT("DataType")))
			{
				if (dataType[1].Equals(TEXT("Slots")))
				{
					tableData.slotRange.first = i + 1;
					state = Slots;
				}
				else if (dataType[1].Equals(TEXT("Properties")))
				{
					tableData.propertyRange.first = i + 1;
					state = Properties;
				}
				else if (dataType[1].Equals(TEXT("StartsInProject")))
				{
					tableData.startInProjectRange.first = i + 1;
					state = StartsInProject;
				}
				else if (dataType[1].Equals(TEXT("InputPins")))
				{
					tableData.pinRange.first = i + 1;
					state = InputPins;
				}
				else if (dataType[1].Contains(TEXT("Parent")))
				{
					tableData.parentPathRange.first = i + 1;
					state = ParentPaths;
				}
				else if (dataType[1].Contains(TEXT("MyNode")))
				{
					tableData.myPathRange.first = i + 1;
					state = MyPath;
				}
				else if (dataType[1].Equals(TEXT("ID")))
				{
					tableData.idRange.first = i + 1;
					state = ID;
				}
				continue;
			}

			switch (state)
			{
				case StartsInProject:
				{
					//Column title doesn't matter, just need any value to check range
					tableData.startInProjectRange.columns.Add(TEXT(""));
				}
				break;

				case Slots:
				{
					if (!cell.IsEmpty())
					{
						tableData.slotRange.columns.Add(cell);
					}
				}
				break;

				case Properties:
				{
					tableData.propertyRange.columns.Add(cell);
				}
				break;

				case InputPins:
				{
					if (tableData.pinRange.columns.Num() < tableData.nodeType.ChildAttachments.Num())
					{
						tableData.pinRange.columns.Add(normalizeCell(*cell));
					}
				}
				break;

				case Configuration:
				{
					tableData.configRange.columns.Add(cell);
				}
				break;

				case ID:
				{
					tableData.idRange.columns.Add(normalizeCell(*cell));
				}
				break;

				case ParentPaths:
				case MyPath:
				{
					TArray<FString> tags;
					cell.RemoveSpacesInline();
					cell.ParseIntoArray(tags, TEXT("-->"));

					FBIMTagPath path;
					for (auto &tag : tags)
					{
						FBIMTagGroup &group = path.AddDefaulted_GetRef();
						group.Add(*tag);
					};

					if (state == MyPath)
					{
						tableData.myPaths.Add(path);
						tableData.myPathRange.columns.Add(cell);
					}
					else
					{
						tableData.parentPaths.Add(path);
						tableData.parentPathRange.columns.Add(cell);
					}
				}
				break;
			};
		}
	});

	processor.AddRule(kPreset, [&OutMessages, &OutStarters, normalizeCell, &tableData, &ManifestFile, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		for (int32 i = 1; i < Row.Num(); ++i)
		{
			FString cell = normalizeCell(Row[i]);

			if (tableData.idRange.IsIn(i))
			{
				if (!cell.IsEmpty())
				{
					if (!tableData.currentPreset.PresetID.IsNone())
					{
						Presets.Add(tableData.currentPreset.PresetID, tableData.currentPreset);
						tableData.currentPreset = FBIMPreset();
					}
					tableData.currentPreset.PresetID = FBIMKey(cell);
					tableData.currentPreset.NodeType = tableData.nodeType.TypeName;
					tableData.currentPreset.NodeScope = tableData.nodeType.Scope;
				}
			}
			else if (tableData.startInProjectRange.IsIn(i))
			{
				if (!cell.IsEmpty())
				{
					OutStarters.Add(tableData.currentPreset.PresetID);
				}
			}
			else if (tableData.propertyRange.IsIn(i))
			{
				FBIMPropertyValue propSpec(*tableData.propertyRange.Get(i));
				// Only add blank properties if they don't already exist
				if (!tableData.currentPreset.HasProperty(propSpec.Name) || !cell.IsEmpty())
				{
					tableData.currentPreset.SetScopedProperty(propSpec.Scope, propSpec.Name, cell);
				}
			}
			else if (tableData.myPathRange.IsIn(i) && !cell.IsEmpty())
			{
				tableData.currentPreset.MyTagPath.FromString(cell);
			}
			else if (tableData.parentPathRange.IsIn(i) && !cell.IsEmpty())
			{
				FBIMTagPath &path = tableData.currentPreset.ParentTagPaths.AddDefaulted_GetRef();
				// TODO: standardize parent paths...matrix of Xs or lists in cells?
				if (cell.Equals(TEXT("X")))
				{
					path.FromString(tableData.parentPathRange.Get(i));
				}
				else
				{
					path.FromString(cell);
				}
			}
			else if (tableData.slotRange.IsIn(i))
			{
				FString category = tableData.slotRange.Get(i);
				if (category.Equals(TEXT("SlotConfig")))
				{
					if (!cell.IsEmpty())
					{
						tableData.currentPreset.SlotConfigPresetID = FBIMKey(normalizeCell(cell));
					}
				}
				else if (category.Equals(TEXT("PartPreset")) && ensureAlways(!cell.IsEmpty()))
				{
					if (tableData.currentPreset.PartSlots.Num() == 0 || !tableData.currentPreset.PartSlots.Last().PartPreset.IsNone())
					{
						tableData.currentPreset.PartSlots.AddDefaulted();
					}
					tableData.currentPreset.PartSlots.Last().PartPreset = FBIMKey(normalizeCell(cell));
				}
				else if (category.Equals(TEXT("ID")))
				{
					if (tableData.currentPreset.PartSlots.Num() == 0 || !tableData.currentPreset.PartSlots.Last().ID.IsNone())
					{
						tableData.currentPreset.PartSlots.AddDefaulted();
					}
					tableData.currentPreset.PartSlots.Last().ID = normalizeCell(cell);
				}
				else if (category.Equals(TEXT("ParentID")))
				{
					if (tableData.currentPreset.PartSlots.Num() == 0 || !tableData.currentPreset.PartSlots.Last().ParentID.IsNone())
					{
						tableData.currentPreset.PartSlots.AddDefaulted();
					}
					tableData.currentPreset.PartSlots.Last().ParentID = normalizeCell(cell);
				}
				else if (category.Equals(TEXT("SlotID")))
				{
					if (tableData.currentPreset.PartSlots.Num() == 0 || !tableData.currentPreset.PartSlots.Last().SlotName.IsNone())
					{
						tableData.currentPreset.PartSlots.AddDefaulted();
					}
					tableData.currentPreset.PartSlots.Last().SlotName = normalizeCell(cell);
				}
			}
			else if (tableData.pinRange.IsIn(i))
			{
				FName pinName = *tableData.pinRange.Get(i);
				bool found = false;
				for (int32 setIndex = 0; setIndex < tableData.nodeType.ChildAttachments.Num(); ++setIndex)
				{
					if (tableData.nodeType.ChildAttachments[setIndex].SetName == pinName)
					{
						int32 setPosition = 0;
						for (auto &cap : tableData.currentPreset.ChildPresets)
						{
							if (cap.ParentPinSetIndex == setIndex)
							{
								setPosition = FMath::Max(cap.ParentPinSetPosition + 1, setPosition);
							}
						}
						FBIMPreset::FChildAttachment &newCAP = tableData.currentPreset.ChildPresets.AddDefaulted_GetRef();
						newCAP.ParentPinSetIndex = setIndex;
						newCAP.ParentPinSetPosition = setPosition;
						FString rowStr = cell;
						rowStr.RemoveSpacesInline();
						newCAP.PresetID = FBIMKey(rowStr);
						found = true;
					}
				}
				if (!found)
				{
					OutMessages.Add(FString::Printf(TEXT("Could not find pin %s in %s"), *tableData.pinRange.Get(i), *ManifestFile));
				}
			}
		}

	});

	processor.AddRule(kInputPin, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		FString extensibility;
		FChildAttachmentType &childAttachment = tableData.nodeType.ChildAttachments.AddDefaulted_GetRef();

		for (int32 i = 1; i < Row.Num(); ++i)
		{
			FString rowStr = Row[i];
			if (!rowStr.IsEmpty())
			{
				if (extensibility.IsEmpty())
				{
					extensibility = rowStr;
				}
				else
				{
					childAttachment.SetName = *rowStr;
					break;
				}
			}
		}

		if (extensibility.IsEmpty())
		{
			OutMessages.Add(FString::Printf(TEXT("Bad input pin")));
		}

		TArray<FString> range;
		FString(extensibility).ParseIntoArray(range, TEXT(".."));
		if (range.Num() == 2)
		{
			childAttachment.MinCount = FCString::Atoi(*range[0]);
			childAttachment.MaxCount = FCString::Atoi(*range[1]);
		}
		else if (range.Num() == 1)
		{
			childAttachment.MinCount = FCString::Atoi(*range[0]);
			childAttachment.MaxCount = -1;
		}
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

			tableData = FTableData();

			if (!processor.ParseFile(*(ManifestPath / file)))
			{
				OutMessages.Add(FString::Printf(TEXT("Failed to load CSV file %s"), *file));
				return ECraftingResult::Error;
			}

			// Make sure the last preset we were reading ends up in the map
			if (!tableData.currentPreset.PresetID.IsNone())
			{
				Presets.Add(tableData.currentPreset.PresetID, tableData.currentPreset);
			}

		}
		return ECraftingResult::Success;
	}
	OutMessages.Add(FString::Printf(TEXT("Failed to load manifest file %s"), *ManifestFile));
	return ECraftingResult::Error;
}
