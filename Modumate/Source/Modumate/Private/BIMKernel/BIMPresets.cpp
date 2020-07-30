// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMPresets.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateScriptProcessor.h"

namespace Modumate { namespace BIM{ 

	bool FChildAttachmentPreset::operator==(const FChildAttachmentPreset &OtherAttachment) const
	{
		return ParentPinSetIndex == OtherAttachment.ParentPinSetIndex &&
			ParentPinSetPosition == OtherAttachment.ParentPinSetPosition &&
			PresetID == OtherAttachment.PresetID;
	}

ECraftingResult FCraftingPresetCollection::GetInstanceDataAsPreset(const FCraftingTreeNodeInstanceSharedPtrConst &Instance, FCraftingTreeNodePreset &OutPreset) const
{
	const FCraftingTreeNodePreset *basePreset = Presets.Find(Instance->PresetID);
	if (basePreset == nullptr)
	{
		return ECraftingResult::Error;
	}

	OutPreset.NodeType = basePreset->NodeType;
	OutPreset.Properties = Instance->InstanceProperties;
	OutPreset.PresetID = basePreset->PresetID;
	OutPreset.MyTagPath = basePreset->MyTagPath;
	OutPreset.ParentTagPaths = basePreset->ParentTagPaths;
	OutPreset.ObjectType = basePreset->ObjectType;
	OutPreset.IconType = basePreset->IconType;
	OutPreset.Orientation = basePreset->Orientation;
	OutPreset.CanFlipOrientation = basePreset->CanFlipOrientation;

	for (int32 pinSetIndex = 0; pinSetIndex < Instance->AttachedChildren.Num(); ++pinSetIndex)
	{
		const FCraftingTreeNodeAttachedChildren &pinSet = Instance->AttachedChildren[pinSetIndex];
		for (int32 pinSetPosition = 0; pinSetPosition < pinSet.Children.Num(); ++pinSetPosition)
		{
			FChildAttachmentPreset &pinSpec = OutPreset.ChildPresets.AddDefaulted_GetRef();
			pinSpec.ParentPinSetIndex = pinSetIndex;
			pinSpec.ParentPinSetPosition = pinSetPosition;

			FCraftingTreeNodeInstanceSharedPtr attachedOb = pinSet.Children[pinSetPosition].Pin();
			pinSpec.PresetID = attachedOb->PresetID;
			const FCraftingTreeNodePreset *attachedPreset = Presets.Find(attachedOb->PresetID);
		}
	}
	OutPreset.SortChildNodes();
	return ECraftingResult::Success;
}

ECraftingResult FCraftingPresetCollection::ToDataRecords(TArray<FCraftingPresetRecord> &OutRecords) const
{
	for (auto &kvp : Presets)
	{
		FCraftingPresetRecord &presetRec = OutRecords.AddDefaulted_GetRef();
		kvp.Value.ToDataRecord(presetRec);
	}
	return ECraftingResult::Success;
}

ECraftingResult FCraftingPresetCollection::FromDataRecords(const TArray<FCraftingPresetRecord> &Records)
{
	Presets.Empty();

	for (auto &presetRecord : Records)
	{
		FCraftingTreeNodePreset newPreset;
		if (newPreset.FromDataRecord(*this, presetRecord) == ECraftingResult::Success)
		{
			Presets.Add(newPreset.PresetID, newPreset);
		}
	}

	return ECraftingResult::Success;
}

bool FCraftingTreeNodePreset::Matches(const FCraftingTreeNodePreset &OtherPreset) const
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

ECraftingResult FCraftingTreeNodePreset::ToDataRecord(FCraftingPresetRecord &OutRecord) const
{
	OutRecord.DisplayName = GetDisplayName();
	OutRecord.NodeType = NodeType;
	OutRecord.PresetID = PresetID;
	MyTagPath.ToString(OutRecord.MyTagPath);

	for (auto &ptp : ParentTagPaths)
	{
		ptp.ToString(OutRecord.ParentTagPaths.AddDefaulted_GetRef());
	}

	for (auto &childPreset : ChildPresets)
	{
		OutRecord.ChildSetIndices.Add(childPreset.ParentPinSetIndex);
		OutRecord.ChildSetPositions.Add(childPreset.ParentPinSetPosition);
		OutRecord.ChildPresets.Add(childPreset.PresetID);
	}

	Properties.ToDataRecord(OutRecord.PropertyRecord);

	return ECraftingResult::Success;
}

ECraftingResult FCraftingTreeNodePreset::SortChildNodes()
{
	ChildPresets.Sort([](const FChildAttachmentPreset &lhs, const FChildAttachmentPreset &rhs)
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

ECraftingResult FCraftingTreeNodePreset::FromDataRecord(const FCraftingPresetCollection &PresetCollection, const FCraftingPresetRecord &Record)
{
	NodeType = Record.NodeType;

	const FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(NodeType);
	// TODO: this ensure will fire if expected presets have become obsolete, resave to fix
	if (!ensureAlways(nodeType != nullptr))
	{
		return ECraftingResult::Error;
	}

	PresetID = Record.PresetID;

	Properties.Empty();
	ChildPresets.Empty();

	Properties.FromDataRecord(Record.PropertyRecord);

	if (ensureAlways(Record.ChildSetIndices.Num() == Record.ChildPresets.Num() &&
		Record.ChildSetPositions.Num() == Record.ChildPresets.Num()))
	{
		for (int32 i = 0; i < Record.ChildPresets.Num(); ++i)
		{
			FChildAttachmentPreset &attachment = ChildPresets.AddDefaulted_GetRef();
			attachment.ParentPinSetIndex = Record.ChildSetIndices[i];
			attachment.ParentPinSetPosition = Record.ChildSetPositions[i];
			attachment.PresetID = Record.ChildPresets[i];
		}
	}

	MyTagPath.FromString(Record.MyTagPath);

	for (auto &ptp : Record.ParentTagPaths)
	{
		ParentTagPaths.AddDefaulted_GetRef().FromString(ptp);
	}

	return ECraftingResult::Success;
}

ECraftingResult FCraftingTreeNodePreset::ToParameterSet(FModumateFunctionParameterSet &OutParameterSet) const
{
	FCraftingPresetRecord dataRecord;
	ECraftingResult result = ToDataRecord(dataRecord);
	if (result != ECraftingResult::Success)
	{
		return result;
	}

	OutParameterSet.SetValue(Modumate::Parameters::kDisplayName, dataRecord.DisplayName);
	OutParameterSet.SetValue(Modumate::Parameters::kNodeType, dataRecord.NodeType);
	OutParameterSet.SetValue(Modumate::Parameters::kPresetKey, dataRecord.PresetID);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetIndices, dataRecord.ChildSetIndices);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetPositions, dataRecord.ChildSetPositions);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetPresetIDs, dataRecord.ChildPresets);
	OutParameterSet.SetValue(Modumate::Parameters::kParentTagPaths, dataRecord.ParentTagPaths);
	OutParameterSet.SetValue(Modumate::Parameters::kMyTagPath, dataRecord.MyTagPath);

	TArray<FString> propertyNames;
	dataRecord.PropertyRecord.Properties.GenerateKeyArray(propertyNames);
	OutParameterSet.SetValue(Modumate::Parameters::kPropertyNames, propertyNames);

	TArray<FString> propertyValues;
	dataRecord.PropertyRecord.Properties.GenerateValueArray(propertyValues);
	OutParameterSet.SetValue(Modumate::Parameters::kPropertyValues, propertyValues);

	return ECraftingResult::Success;
}

ECraftingResult FCraftingTreeNodePreset::FromParameterSet(const FCraftingPresetCollection &PresetCollection, const FModumateFunctionParameterSet &ParameterSet)
{
	FCraftingPresetRecord dataRecord;

	// Get base properties, including property bindings that set one property's value to another
	dataRecord.NodeType = ParameterSet.GetValue(Modumate::Parameters::kNodeType);
	const FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(dataRecord.NodeType);
	if (ensureAlways(nodeType != nullptr))
	{
		nodeType->Properties.ToDataRecord(dataRecord.PropertyRecord);
	}

	// Get local overrides of properties
	TArray<FString> propertyNames = ParameterSet.GetValue(Modumate::Parameters::kPropertyNames);
	TArray<FString> propertyValues = ParameterSet.GetValue(Modumate::Parameters::kPropertyValues);

	if (!ensureAlways(propertyNames.Num() == propertyValues.Num()))
	{
		return ECraftingResult::Error;
	}

	int32 numProps = propertyNames.Num();
	for (int32 i = 0; i < numProps; ++i)
	{
		dataRecord.PropertyRecord.Properties.Add(propertyNames[i], propertyValues[i]);
	}

	dataRecord.DisplayName = ParameterSet.GetValue(Modumate::Parameters::kDisplayName);
	dataRecord.PresetID = ParameterSet.GetValue(Modumate::Parameters::kPresetKey);
	dataRecord.ChildSetIndices = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetIndices);
	dataRecord.ChildSetPositions = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPositions);
	dataRecord.ChildPresets = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPresetIDs);
	dataRecord.ParentTagPaths = ParameterSet.GetValue(Modumate::Parameters::kParentTagPaths);
	dataRecord.MyTagPath = ParameterSet.GetValue(Modumate::Parameters::kMyTagPath);

	return FromDataRecord(PresetCollection, dataRecord);
}

/*
Given a preset ID, recurse through all its children and gather all other presets that this one depends on
Note: we don't empty the container because the function is recursive
*/
ECraftingResult FCraftingPresetCollection::GetDependentPresets(const FName &PresetID, TSet<FName> &OutPresets) const
{
	const FCraftingTreeNodePreset *preset = Presets.Find(PresetID);
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

FString FCraftingTreeNodePreset::GetDisplayName() const
{
	return Properties.GetProperty(BIM::EScope::Preset, BIM::Parameters::Name);
}

EObjectType FCraftingPresetCollection::GetPresetObjectType(const FName &PresetID) const
{
	const FCraftingTreeNodePreset *preset = Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return EObjectType::OTNone;
	}
	return preset->ObjectType;
}

// TODO: Loading data from a CSV manifest file is not going to be required long term. 
// Ultimately we will develop a compiler from this code that generates a record that can be read more efficiently
// Once this compiler is in the shape we intend, we will determine where in the toolchain this code will reside we can refactor for long term sustainability
// Until then this is a prototypical development space used to prototype the relational database structure being authored in Excel
ECraftingResult FCraftingPresetCollection::LoadCSVManifest(const FString &ManifestPath, const FString &ManifestFile, TArray<FString> &OutMessages)
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
			return i >= first && i < first + columns.Num();
		}

		const FString &Get(int32 i) const {
			return columns[i - first];
		}
	};

	struct FTableData
	{
		FCraftingTreeNodeType nodeType;
		TArray<FTagPath> myPaths;
		TArray<FTagPath> parentPaths;

		FCraftingTreeNodePreset currentPreset;

		FColumnRange configRange, propertyRange, myPathRange, parentPathRange, pinRange, idRange;
	};

	FTableData tableData;

	auto normalizeCell = [](const TCHAR *Row)
	{
		FString cell;
		int32 sl = FCString::Strlen(Row);
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
		for (int32 i = 1; i < Row.Num(); ++i)
		{
			if (!FString(Row[i]).IsEmpty())
			{
				tableData.nodeType.TypeName = Row[i];
				break;
			}
		}
	});

	processor.AddRule(kProperty, [&OutMessages, &tableData](const TArray<const TCHAR*> &Row, int32 RowNumber)
	{
		FName propertyType;
		FString propertyName;
		FValueSpec propertyValue;

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
					propertyValue = FValueSpec(Row[i]);
					break;
				}
			}
		}

		if (propertyValue.Scope == EScope::None)
		{
			OutMessages.Add(FString::Printf(TEXT("Bad property")));
		}
		else
		{
			tableData.nodeType.Properties.SetProperty(propertyValue.Scope, propertyValue.Name, Modumate::BIM::FValue());
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
			MyPath,
			Slots
		};

		State state = Configuration;

		tableData.configRange.first = 1;

		for (int32 i = 1; i < Row.Num(); ++i)
		{
			FString cell = Row[i];

			if (cell.Contains(TEXT("DataType=")))
			{
				if (cell.Contains(TEXT("Slots")))
				{
					state = Slots;
				}
				else if (cell.Contains(TEXT("Properties")))
				{
					tableData.propertyRange.first = i + 1;
					state = Properties;
				}
				else if (cell.Contains(TEXT("InputPins")))
				{
					tableData.pinRange.first = i + 1;
					state = InputPins;
				}
				else if (cell.Contains(TEXT("Parent")))
				{
					tableData.parentPathRange.first = i + 1;
					state = ParentPaths;
				}
				else if (cell.Contains(TEXT("MyNode")))
				{
					tableData.myPathRange.first = i + 1;
					state = MyPath;
				}
				else if (cell.Contains(TEXT("=ID")))
				{
					tableData.idRange.first = i + 1;
					state = ID;
				}
				continue;
			}

			switch (state)
			{
			case Slots:
				break;

			case Properties:
			{
				tableData.propertyRange.columns.Add(cell);
			}
			break;

			case InputPins:
			{
				tableData.pinRange.columns.Add(normalizeCell(*cell));
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

				FTagPath path;
				for (auto &tag : tags)
				{
					FTagGroup &group = path.AddDefaulted_GetRef();
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

	processor.AddRule(kPreset, [&OutMessages, normalizeCell, &tableData, &ManifestFile, this](const TArray<const TCHAR*> &Row, int32 RowNumber)
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
						tableData.currentPreset = FCraftingTreeNodePreset();
					}
					tableData.currentPreset.PresetID = *cell;
					tableData.currentPreset.NodeType = tableData.nodeType.TypeName;
				}
			}
			else if (tableData.propertyRange.IsIn(i))
			{
				FValueSpec propSpec(*tableData.propertyRange.Get(i));
				tableData.currentPreset.Properties.SetProperty(propSpec.Scope, propSpec.Name, cell);
			}
			else if (tableData.myPathRange.IsIn(i) && !cell.IsEmpty())
			{
				tableData.currentPreset.MyTagPath.FromString(cell);
			}
			else if (tableData.parentPathRange.IsIn(i) && !cell.IsEmpty())
			{
				FTagPath &path = tableData.currentPreset.ParentTagPaths.AddDefaulted_GetRef();
				path.FromString(tableData.parentPathRange.Get(i));
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
						FChildAttachmentPreset &newCAP = tableData.currentPreset.ChildPresets.AddDefaulted_GetRef();
						newCAP.ParentPinSetIndex = setIndex;
						newCAP.ParentPinSetPosition = setPosition;
						FString rowStr = cell;
						rowStr.RemoveSpacesInline();
						newCAP.PresetID = *rowStr;
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
		}
		return ECraftingResult::Success;
	}
	OutMessages.Add(FString::Printf(TEXT("Failed to load manifest file %s"), *ManifestFile));
	return ECraftingResult::Error;
}

} }