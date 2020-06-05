// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMDataModel.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Database/ModumateDataTables.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "Algo/Compare.h"
#include "Algo/Accumulate.h"
#include "Algo/Transform.h"

namespace Modumate { namespace BIM {
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
	OutPreset.Tags = basePreset->Tags;

	for (int32 pinSetIndex = 0; pinSetIndex < Instance->InputPins.Num(); ++pinSetIndex)
	{
		const FCraftingTreeNodePinSet &pinSet = Instance->InputPins[pinSetIndex];
		for (int32 pinSetPosition = 0; pinSetPosition < pinSet.AttachedObjects.Num(); ++pinSetPosition)
		{
			FCraftingTreeNodePreset::FPinSpec &pinSpec = OutPreset.ChildNodes.AddDefaulted_GetRef();
			pinSpec.PinSetIndex = pinSetIndex;
			pinSpec.Scope = pinSet.Scope;
			pinSpec.PinSetPosition = pinSetPosition;
			pinSpec.PinSpecSearchTags = Instance->InputPins[pinSetIndex].EligiblePresetSearch.SelectionSearchTags;

			FCraftingTreeNodeInstanceSharedPtr attachedOb = pinSet.AttachedObjects[pinSetPosition].Pin();

			// If an attached object is ReadOnly, it is a category selector
			// In this case, iterate down the child list of ReadOnlies and record them all in the pin sequence
			while (attachedOb.IsValid())
			{
				const FCraftingTreeNodePreset *attachedPreset = Presets.Find(attachedOb->PresetID);

				if (ensureAlways(attachedPreset != nullptr))
				{
					FCraftingPresetSearchSelection *childValue = &pinSpec.PresetSequence.AddDefaulted_GetRef();
					childValue->NodeType = attachedPreset->NodeType;
					childValue->SelectedPresetID = attachedOb->PresetID;

					//We hit a writeable preset, so we're done
					if (!attachedPreset->IsReadOnly)
					{
						attachedOb = nullptr;
					}
					else
					{
						// Otherwise ensure this ReadOnly preset has only one child and iterate
						if (ensureAlways(attachedOb->InputPins.Num() == 1 && attachedOb->InputPins[0].AttachedObjects.Num() == 1))
						{
							attachedOb = attachedOb->InputPins[0].AttachedObjects[0].Pin();
						}
						else
						{
							attachedOb = nullptr;
						}
					}
				}
				else
				{
					attachedOb = nullptr;
				}
			}
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

FCraftingPresetTag::FCraftingPresetTag(const FString &InQualifiedString) : QualifiedString(InQualifiedString)
{
	FString scopeStr, tagStr;
	if (ensureAlways(QualifiedString.Split(TEXT("."), &scopeStr, &tagStr)))
	{
		Scope = *scopeStr;
		Tag = *tagStr;
	}
}

FCraftingPresetTag::FCraftingPresetTag(const FName &InScope, const FName &InTag) : Scope(InScope), Tag(InTag)
{
	QualifiedString = FString::Printf(TEXT("%s.%s"), *Scope.ToString(), *Tag.ToString());
}

bool FCraftingTreeNodePreset::HasTag(const FCraftingPresetTag &Tag) const
{
	const TArray<FCraftingPresetTag> *group = Tags.Find(Tag.GetScope());
	if (group != nullptr)
	{
		return group->Contains(Tag);
	}
	return false;
}

bool FCraftingTreeNodePreset::Matches(const FCraftingTreeNodePreset &OtherPreset) const
{
	if (!Properties.Matches(OtherPreset.Properties))
	{
		return false;
	}

	FCraftingPresetRecord thisRecord, thatRecord;
	ToDataRecord(thisRecord);
	OtherPreset.ToDataRecord(thatRecord);

	if (thisRecord.DisplayName != thatRecord.DisplayName)
	{
		return false;
	}

	if (thisRecord.NodeType != thatRecord.NodeType)
	{
		return false;
	}

	if (thisRecord.PresetID != thatRecord.PresetID)
	{
		return false;
	}

	if (thisRecord.IsReadOnly != thatRecord.IsReadOnly)
	{
		return false;
	}

	if (thisRecord.ChildNodePinSetIndices.Num() != thatRecord.ChildNodePinSetIndices.Num())
	{
		return false;
	}

	// If we pass the indices count test above, we should always pass this one
	if (!ensureAlways(thisRecord.ChildNodePinSetPositions.Num() == thatRecord.ChildNodePinSetPositions.Num()))
	{
		return false;
	}

#define CRAFTING_ARRAY_CHECK(Type,Array) if (!Algo::CompareByPredicate(thisRecord.##Array, thatRecord.##Array,[](const Type &thisVal, const Type &thatVal){return thisVal==thatVal;})){return false;}

	CRAFTING_ARRAY_CHECK(int32, ChildNodePinSetIndices);
	CRAFTING_ARRAY_CHECK(int32, ChildNodePinSetPositions);
	CRAFTING_ARRAY_CHECK(FName, ChildNodePinSetPresetIDs);
	CRAFTING_ARRAY_CHECK(FName, ChildNodePinSetNodeTypes);
	CRAFTING_ARRAY_CHECK(FString, ChildNodePinSetTags);

#undef CRAFTING_ARRAY_CHECK

	return true;
}

ECraftingResult FCraftingTreeNodePreset::ToDataRecord(FCraftingPresetRecord &OutRecord) const
{
	OutRecord.DisplayName = GetDisplayName();
	OutRecord.NodeType = NodeType;
	OutRecord.PresetID = PresetID;
	OutRecord.IsReadOnly = IsReadOnly;

	for (const auto &kvp : Tags)
	{
		Algo::Transform(kvp.Value, OutRecord.Tags, [](const FCraftingPresetTag &Tag) {return Tag.GetQualifiedString(); });
	}

	Properties.ToDataRecord(OutRecord.PropertyRecord);

	for (auto &childNode : ChildNodes)
	{
		for (int32 i = 0; i < childNode.PresetSequence.Num(); ++i)
		{
			for (int32 j = 0; j < childNode.PinSpecSearchTags.Num(); ++j)
			{
				OutRecord.ChildNodePinSetIndices.Add(childNode.PinSetIndex);
				OutRecord.ChildNodePinSetPositions.Add(childNode.PinSetPosition);
				OutRecord.ChildNodePinSetPresetIDs.Add(childNode.PresetSequence[i].SelectedPresetID);
				OutRecord.ChildNodePinSetTags.Add(childNode.PinSpecSearchTags[j].GetQualifiedString());
				OutRecord.ChildNodePinSetNodeTypes.Add(childNode.PresetSequence[i].NodeType);
			}
		}
	}

	return ECraftingResult::Success;
}

ECraftingResult FCraftingTreeNodePreset::SortChildNodes()
{
	ChildNodes.Sort([](const FPinSpec &lhs, const FPinSpec &rhs)
	{
		if (lhs.PinSetIndex < rhs.PinSetIndex)
		{
			return true;
		}
		if (lhs.PinSetIndex > rhs.PinSetIndex)
		{
			return false;
		}
		return lhs.PinSetPosition < rhs.PinSetPosition;
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
	IsReadOnly = Record.IsReadOnly;
	Tags.Empty();

	for (auto &tag : Record.Tags)
	{
		FCraftingPresetTag newTag(tag);
		TArray<FCraftingPresetTag> &group = Tags.FindOrAdd(newTag.GetScope());
		group.Add(newTag);
	}

	Properties.Empty();
	ChildNodes.Empty();

	Properties.FromDataRecord(Record.PropertyRecord);

	int32 numChildren = Record.ChildNodePinSetIndices.Num();
	if (ensureAlways(
		numChildren == Record.ChildNodePinSetPositions.Num() &&
		numChildren == Record.ChildNodePinSetPresetIDs.Num() &&
		numChildren == Record.ChildNodePinSetNodeTypes.Num() &&
		numChildren == Record.ChildNodePinSetTags.Num()))
	{
		FPinSpec *pinSpec = nullptr;
		int32 i = 0;
		while (i < numChildren)
		{
			// Pin specs hold sequences of child presets
			// A sequence is a list of zero or more ReadOnly presets followed by a single writable terminating preset
			// When the record contains the same set index/set position pair multiple times in a row, this defines a sequence
			pinSpec = &ChildNodes.AddDefaulted_GetRef();
			pinSpec->PinSetIndex = Record.ChildNodePinSetIndices[i];
			pinSpec->PinSetPosition = Record.ChildNodePinSetPositions[i];

			if (ensureAlways(pinSpec->PinSetIndex < nodeType->InputPins.Num()))
			{
				pinSpec->Scope = nodeType->InputPins[pinSpec->PinSetIndex].Scope;
			}

			FCraftingPresetSearchSelection *childSequenceValue = &pinSpec->PresetSequence.AddDefaulted_GetRef();

			childSequenceValue->NodeType = Record.ChildNodePinSetNodeTypes[i];
			childSequenceValue->SelectedPresetID = Record.ChildNodePinSetPresetIDs[i];

			while (i < numChildren &&
				pinSpec->PinSetIndex == Record.ChildNodePinSetIndices[i] &&
				pinSpec->PinSetPosition == Record.ChildNodePinSetPositions[i])
			{
				if (childSequenceValue->SelectedPresetID != Record.ChildNodePinSetPresetIDs[i] ||
					childSequenceValue->NodeType != Record.ChildNodePinSetNodeTypes[i])
				{
					childSequenceValue = &pinSpec->PresetSequence.AddDefaulted_GetRef();
					childSequenceValue->NodeType = Record.ChildNodePinSetNodeTypes[i];
					childSequenceValue->SelectedPresetID = Record.ChildNodePinSetPresetIDs[i];
				}
				pinSpec->PinSpecSearchTags.AddUnique(FCraftingPresetTag(Record.ChildNodePinSetTags[i]));
				++i;
			}
		}
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
	OutParameterSet.SetValue(Modumate::Parameters::kReadOnly, dataRecord.IsReadOnly);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetIndices, dataRecord.ChildNodePinSetIndices);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetPositions, dataRecord.ChildNodePinSetPositions);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetPresetIDs, dataRecord.ChildNodePinSetPresetIDs);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetNodeTypes, dataRecord.ChildNodePinSetNodeTypes);
	OutParameterSet.SetValue(Modumate::Parameters::kChildNodePinSetTags, dataRecord.ChildNodePinSetTags);
	OutParameterSet.SetValue(Modumate::Parameters::kTags, dataRecord.Tags);

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
	dataRecord.IsReadOnly = ParameterSet.GetValue(Modumate::Parameters::kReadOnly);
	dataRecord.ChildNodePinSetIndices = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetIndices);
	dataRecord.ChildNodePinSetPositions = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPositions);
	dataRecord.ChildNodePinSetPresetIDs = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetPresetIDs);
	dataRecord.ChildNodePinSetNodeTypes = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetNodeTypes);
	dataRecord.ChildNodePinSetTags = ParameterSet.GetValue(Modumate::Parameters::kChildNodePinSetTags);
	dataRecord.Tags = ParameterSet.GetValue(Modumate::Parameters::kTags);

	return FromDataRecord(PresetCollection, dataRecord);
}

ECraftingResult FCraftingPresetCollection::SearchPresets(
	const FName &NodeType,
	const TArray<FCraftingPresetTag> &IncludedTags,
	const TArray<FCraftingPresetTag> &ExcludedTags,
	TArray<FName> &OutPresets) const
{
	TArray<FCraftingTreeNodePreset> qualifiers;
	if (NodeType.IsNone())
	{
		for (auto &kvp : Presets)
		{
			qualifiers.Add(kvp.Value);
		}
	}
	else
	{
		for (auto &kvp : Presets)
		{
			if (kvp.Value.NodeType == NodeType)
			{
				qualifiers.Add(kvp.Value);
			}
		}
	}

	qualifiers = qualifiers.FilterByPredicate([&IncludedTags, &ExcludedTags](const FCraftingTreeNodePreset &Preset)
	{
		for (auto &excluded : ExcludedTags)
		{
			if (Preset.HasTag(excluded))
			{
				return false;
			}
		}
		if (IncludedTags.Num() == 0)
		{
			return true;
		}
		for (auto &included : IncludedTags)
		{
			if (Preset.HasTag(included))
			{
				return true;
			}
		}
		return false;
	});

	Algo::Transform(qualifiers, OutPresets, [](const FCraftingTreeNodePreset &Preset) {return Preset.PresetID; });

	return ECraftingResult::Success;
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

	for (auto &childNode : preset->ChildNodes)
	{
		/*
		Only recurse if we haven't processed this ID yet
		Even if the same preset appears multiple times in a tree, its children will always be the same
		*/
		for (auto &fieldValue : childNode.PresetSequence)
		{
			if (!OutPresets.Contains(fieldValue.SelectedPresetID))
			{
				OutPresets.Add(fieldValue.SelectedPresetID);
				GetDependentPresets(fieldValue.SelectedPresetID, OutPresets);
			}
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
	const FCraftingTreeNodeType *nodeType = NodeDescriptors.Find(preset->NodeType);
	if (ensureAlways(nodeType != nullptr))
	{
		return nodeType->ObjectType;
	}
	return EObjectType::OTNone;
}

ECraftingResult FCraftingPresetCollection::ReadDataTable(UDataTable *DataTable, TArray<FString> &OutMessages)
{
	if (!ensureAlways(DataTable))
	{
		return ECraftingResult::Error;
	}

	enum ETableReadState
	{
		Neutral,
		ReadingDefinition,
		ReadingPresets,
		ReadingLists
	};

	ETableReadState state = ETableReadState::Neutral;

	FCraftingTreeNodeType nodeType;

	TArray<FString> propertyTypes = { TEXT("PRIVATE"),TEXT("STRING"),TEXT("DIMENSION") };

	TArray<FValueSpec> propList;

	DataTable->ForeachRow<FTenColumnTable>(TEXT("FTenColumnTable"),
		[this, &state, &nodeType, &OutMessages, &propList, propertyTypes](
			const FName &Key,
			const FTenColumnTable &data)
	{
		if (state == ETableReadState::Neutral)
		{
			if (!data.C0.IsEmpty())
			{
				nodeType.TypeName = *data.C0;
				state = ETableReadState::ReadingDefinition;
			}
		}
		else if (state == ETableReadState::ReadingDefinition)
		{
			if (data.C0.IsEmpty())
			{
				return;
			}
			if (data.C0 == TEXT("ICON"))
			{
				nodeType.IconType = FindEnumValueByName<EConfiguratorNodeIconType>(TEXT("EConfiguratorNodeIconType"), *data.C1);
			}
			else
				if (propertyTypes.Contains(data.C0))
				{
					if (data.C1.IsEmpty() || data.C2.IsEmpty())
					{
						OutMessages.Add(FString::Printf(TEXT("Bad property format at %s"), *Key.ToString()));
					}
					else
					{
						FValueSpec spec(*data.C2);
						nodeType.Properties.SetProperty(spec.Scope, spec.Name, TEXT(""));
						if (data.C0 != TEXT("PRIVATE"))
						{
							nodeType.FormItemToProperty.Add(data.C1, spec.QN());
						}
					}
				}
				else if (data.C0 == TEXT("Presets"))
				{
					NodeDescriptors.Add(nodeType.TypeName, nodeType);
					state = ETableReadState::ReadingPresets;
				}
		}
		else if (state == ETableReadState::ReadingPresets)
		{
			if (data.C0 == TEXT("Lists"))
			{
				state = ETableReadState::ReadingLists;
			}
			else
				if (data.C0 == TEXT("ID"))
				{
					propList = {
						FValueSpec(*data.C1),FValueSpec(*data.C2),FValueSpec(*data.C3),
						FValueSpec(*data.C4),FValueSpec(*data.C5),FValueSpec(*data.C6),
						FValueSpec(*data.C7),FValueSpec(*data.C8),FValueSpec(*data.C9) };

				}
				else if (data.C0.Mid(0, 2) != TEXT("--"))
				{
					if (propList.Num() == 0)
					{
						OutMessages.Add(FString::Printf(TEXT("No structure for preset at %s"), *Key.ToString()));
					}

					if (data.C0.IsEmpty())
					{
						return;
					}

					FCraftingTreeNodePreset newPreset;

					if (nodeType.TypeName.IsNone())
					{
						OutMessages.Add(FString::Printf(TEXT("Found preset for unidentified node type %s"), *Key.ToString()));
						return;
					}

					newPreset.NodeType = nodeType.TypeName;
					newPreset.PresetID = *data.C0;

					if (Presets.Contains(newPreset.PresetID))
					{
						OutMessages.Add(FString::Printf(TEXT("Preset %s redfined at %s"), *newPreset.PresetID.ToString(), *Key.ToString()));
						return;
					}

					TArray<FString> values = { data.C1,data.C2,data.C3,data.C4,data.C5,data.C6,data.C7,data.C8,data.C9 };

					for (int32 i = 0; i < values.Num(); ++i)
					{
						if (propList[i].Scope == EScope::None)
						{
							break;
						}
						if (nodeType.Properties.HasProperty(propList[i].Scope, propList[i].Name))
						{
							newPreset.Properties.SetProperty(propList[i].Scope, propList[i].Name, values[i]);
						}
						else
						{
							OutMessages.Add(FString::Printf(TEXT("Unidentified property for preset at %s"), *Key.ToString()));
						}
					}

					Presets.Add(newPreset.PresetID, newPreset);
				}
		}
		else if (state == ETableReadState::ReadingLists)
		{
			if (data.C0 == TEXT("ID"))
			{
				return;
			}

			if (data.C0.IsEmpty())
			{
				return;
			}

			if (Lists.Contains(*data.C0))
			{
				OutMessages.Add(FString::Printf(TEXT("List %s redefined at %s"), *data.C0, *Key.ToString()));
				return;
			}

			TArray<FString> stringMembers;
			TArray<FName> nameMembers;
			data.C2.ParseIntoArray(stringMembers, TEXT(","));
			Algo::Transform(stringMembers, nameMembers, [](const FString &Member) {return *Member; });
			Lists.Add(*data.C0, nameMembers);
		}
	});
	return ECraftingResult::Success;
}

ECraftingResult FCraftingPresetCollection::ParseScriptFile(const FString &FilePath, TArray<FString> &OutMessages)
{
	FModumateScriptProcessor compiler;

	static const FString kDefine = TEXT("DEFINE");
	static const FString kEnd = TEXT("END");
	static const FString kInput = TEXT("INPUT:");
	static const FString kScope = TEXT("SCOPE:");
	static const FString kOutput = TEXT("OUTPUT:");
	static const FString kIconType = TEXT("ICON:");
	static const FString kProperties = TEXT("PROPERTIES:");
	static const FString kNodeName = TEXT("[A-Z][A-Z0-9_]+");
	static const FString kNumberMatch = TEXT("[0-9]+");
	static const FString kWhitespace = TEXT("\\s*");
	static const FString kElipsesMatch = TEXT("\\.\\.");

	static const FString kVariable = TEXT("\\w+\\.\\w+");
	static const FString kSimpleWord = TEXT("[\\w\\-]+");

	static const FString kDeclare = TEXT("DECLARE");

	enum State {
		Neutral,
		ReadingProperties,
		ReadingPins,
		ReadingDefinition,
		ReadingPreset,
		ReadingList
	};

	State state = Neutral;

	FCraftingTreeNodeType currentNode;
	FCraftingTreeNodePreset currentPreset;
	TArray<FName> currentListElements;
	FName currentListName;
	TArray<FString> currentListTags;

	//Match END
	compiler.AddRule(kEnd, [&state, &currentNode, &currentListTags, &currentPreset, &currentListName, &currentListElements, this](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state == ReadingDefinition || state == ReadingProperties || state == ReadingPins)
		{
			if (NodeDescriptors.Contains(currentNode.TypeName))
			{
				errorReporter(*FString::Printf(TEXT("Node %s redefined at line %d"), *currentNode.TypeName.ToString(), lineNum));
			}
			else
			{
				if (currentNode.TypeName != NAME_None)
				{
					NodeDescriptors.Add(currentNode.TypeName, currentNode);
				}
				else
				{
					errorReporter(*FString::Printf(TEXT("Unnamed NODE END at line %d"), lineNum));
				}
			}
		}
		else if (state == ReadingList)
		{
			Lists.Add(currentListName, currentListElements);
			for (auto &element : currentListElements)
			{
				FCraftingTreeNodePreset *preset = Presets.Find(element);
				if (preset == nullptr)
				{
					errorReporter(*FString::Printf(TEXT("Unidentified preset %s at %d"), *element.ToString(), lineNum));
				}
				else
				{
					for (auto &tag : currentListTags)
					{
						FCraftingPresetTag tagOb(tag);
						TArray<FCraftingPresetTag> &tagArray = preset->Tags.FindOrAdd(tagOb.GetScope());
						tagArray.Add(tagOb);
					}
				}
			}

			TArray<FCraftingPresetTag> &tags = ListTags.Add(currentListName);
			for (auto &tag : currentListTags)
			{
				FCraftingPresetTag tagOb(tag);
				tags.Add(tagOb);
			}

			currentListElements.Reset();
		}
		else if (state == ReadingPreset)
		{
			// TODO validate complete preset
			currentPreset.SortChildNodes();
			Presets.Add(currentPreset.PresetID, currentPreset);
		}
		else
		{
			errorReporter(*FString::Printf(TEXT("Unexpected END at line %d"), lineNum));
		}
		state = Neutral;
	},
		false);

	//Match INPUT:
	compiler.AddRule(kInput, [&state, &currentNode, this](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingDefinition && state != ReadingProperties)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected INPUT at line %d"), lineNum));
		}
		state = ReadingPins;
	});

	//Match OUTPUT: <object types> ie 'OUTPUT: OTStructureLine'
	compiler.AddRule(kOutput, [&state, &currentNode, this](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingDefinition && state != ReadingProperties && state != ReadingPins)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected OUTPUT at line %d"), lineNum));
		}

		TArray<FString> suffices;
		FString(match.suffix().str().c_str()).ParseIntoArray(suffices, TEXT(" "));
		for (auto &suffix : suffices)
		{
			if (!TryEnumValueByString(EObjectType, suffix, currentNode.ObjectType))
			{
				errorReporter(*FString::Printf(TEXT("Unidentified object type %s at line %d"), *suffix, lineNum));
			}
		}
		state = ReadingDefinition;
	}, true);

	static const FString kList = TEXT("LIST");
	static const FString kPropertyMatch = TEXT("\\w+\\.\\w+");
	static const FString kTagListRequired = TEXT("(") + kPropertyMatch + TEXT("(,") + kPropertyMatch + TEXT(")*)");
	static const FString kListMatch = kList + kWhitespace + TEXT("(") + kSimpleWord + TEXT(")") + kWhitespace + TEXT("(\\(") + kTagListRequired + TEXT("\\))?");

	compiler.AddRule(kListMatch, [this, &state, &currentListTags, &currentListName, &currentListElements](const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != Neutral)
		{
			// No need to bail, but log the error
			errorReporter(*FString::Printf(TEXT("Unexpected LIST definition at line %d"), lineNum));
		}

		if (match.size() < 2)
		{
			errorReporter(*FString::Printf(TEXT("Malformed LIST definition at line %d"), lineNum));
			return;
		}

		currentListName = match[1].str().c_str();
		if (Lists.Contains(currentListName))
		{
			errorReporter(*FString::Printf(TEXT("List %s redefined at line %d"), *currentListName.ToString(), lineNum));
			return;
		}

		currentListTags.Empty();
		if (match.size() > 3 && match[3].matched)
		{
			FString tagStr = match[3].str().c_str();
			tagStr.ParseIntoArray(currentListTags, TEXT(","));
		}

		state = ReadingList;
	}, false);

	static const FString kListElementMatch = kSimpleWord + TEXT("(,") + kWhitespace + kSimpleWord + TEXT(")*");

	compiler.AddRule(kListElementMatch, [this, &state, &currentListElements](const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingList)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected LIST element list at line %d"), lineNum));
		}
		FString elementStr = match[0].str().c_str();
		TArray<FString> elements;
		elementStr.ParseIntoArray(elements, TEXT(","));
		for (auto &element : elements)
		{
			FName elementName = *element.TrimStartAndEnd();
			FCraftingTreeNodePreset *preset = Presets.Find(elementName);
			if (preset == nullptr)
			{
				errorReporter(*FString::Printf(TEXT("Unidentified list element %s at line %d"), *element, lineNum));
			}

			currentListElements.Add(elementName);
		}
	}, false);

	static const FString kListAssignment = TEXT("(") + kSimpleWord + TEXT(")=(") + kSimpleWord + TEXT("\\.") + kSimpleWord + TEXT(")");
	compiler.AddRule(kListAssignment, [this, &state, &currentPreset](const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingPreset)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected list assignment at line %d"), lineNum));
			return;
		}
		if (match.size() < 3)
		{
			errorReporter(*FString::Printf(TEXT("Badly formed variable assignment at line %d"), lineNum));
			return;
		}

		FString listDefault = match[2].str().c_str();
		TArray<FString> listDefaultSplit;
		listDefault.ParseIntoArray(listDefaultSplit, TEXT("."));

		FName nodeTarget = match[1].str().c_str();

		FName listID = *listDefaultSplit[0];
		FName listValue = *listDefaultSplit[1];

		TArray<FName> *list = Lists.Find(listID);

		if (list == nullptr)
		{
			errorReporter(*FString::Printf(TEXT("Unidentified list %s line %d"), *listID.ToString(), lineNum));
			return;
		}

		if (!list->Contains(listValue))
		{
			errorReporter(*FString::Printf(TEXT("Unidentified member %s of list %s line %d"), *listValue.ToString(), *listID.ToString(), lineNum));
			return;
		}

		FCraftingTreeNodePreset *preset = Presets.Find(listValue);

		if (preset == nullptr)
		{
			errorReporter(*FString::Printf(TEXT("Unidentified preset %s in list %s line %d"), *listValue.ToString(), *listID.ToString(), lineNum));
			return;
		}

		FCraftingTreeNodeType *nodeType = NodeDescriptors.Find(currentPreset.NodeType);
		if (nodeType == nullptr)
		{
			errorReporter(*FString::Printf(TEXT("Unidentified preset node type %s line %d"), *currentPreset.NodeType.ToString(), lineNum));
			return;
		}

		TArray<FCraftingPresetTag> *searchTags = ListTags.Find(listID);

		if (searchTags == nullptr)
		{
			errorReporter(*FString::Printf(TEXT("Attempt to assign to untagged list at %d"), lineNum));
			return;
		}

		bool foundSet = false;
		for (int32 i = 0; i < nodeType->InputPins.Num(); ++i)
		{
			if (nodeType->InputPins[i].SetName == nodeTarget)
			{
				// New children are added to front of list
				for (auto &childNode : currentPreset.ChildNodes)
				{
					if (childNode.PinSetIndex == i)
					{
						++childNode.PinSetPosition;
					}
				}

				FCraftingTreeNodePreset::FPinSpec newSpec;
				newSpec.PinSetIndex = i;
				newSpec.PinSetPosition = 0;
				newSpec.Scope = nodeType->InputPins[i].Scope;
				newSpec.PinSpecSearchTags = *searchTags;

				TArray<FString> presetPath;
				int32 presetPathIndex = 0;
				if (match.suffix().length() > 0)
				{
					FString pathStr = match.suffix().str().c_str();
					pathStr.ParseIntoArray(presetPath, TEXT(":"));
				}

				preset = Presets.Find(listValue);

				while (preset != nullptr)
				{
					FCraftingPresetSearchSelection &child = newSpec.PresetSequence.AddDefaulted_GetRef();
					child.SelectedPresetID = preset->PresetID;
					child.NodeType = preset->NodeType;

					if (preset->IsReadOnly)
					{
						if (preset->ChildNodes.Num() != 1 || preset->ChildNodes[0].PresetSequence.Num() < 1)
						{
							errorReporter(*FString::Printf(TEXT("Bad ReadOnly preset at %d"), lineNum));
							preset = nullptr;
						}
						else
						{
							if (presetPathIndex < presetPath.Num())
							{
								preset = Presets.Find(*presetPath[presetPathIndex]);
								++presetPathIndex;
							}
							else
							{
								preset = Presets.Find(preset->ChildNodes[0].PresetSequence[0].SelectedPresetID);
							}
						}
					}
					else
					{
						preset = nullptr;
					}
				}

				currentPreset.ChildNodes.Add(newSpec);
				foundSet = true;
			}
		}

		if (!foundSet)
		{
			errorReporter(*FString::Printf(TEXT("Unidentified list target %s line %d"), *nodeTarget.ToString(), lineNum));
		}
	}, true);


	//Match DEFINE NODENAME
	static const FString kDefineNodePattern = kDefine + kWhitespace + TEXT("(") + kNodeName + TEXT(")");
	compiler.AddRule(kDefineNodePattern, [&state, &currentNode](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != Neutral)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected DEFINE at line %d"), lineNum));
		}
		if (match.size() == 2)
		{
			currentNode = FCraftingTreeNodeType();
			currentNode.TypeName = match[1].str().c_str();
		}
		else
		{
			errorReporter(*FString::Printf(TEXT("Malformed DEFINE at line %d"), lineNum));
		}

		state = ReadingDefinition;
	},
		false);

	//Match PROPERTIES:
	compiler.AddRule(kProperties, [&state](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingDefinition && state != ReadingPins)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected PROPERTIES at line %d"), lineNum));
		}
		state = ReadingProperties;
	},
		false);

	//Match ICON:<iconType>
	static const FString kIconMatch = kIconType + kWhitespace + TEXT("(") + kSimpleWord + TEXT(")");
	compiler.AddRule(kIconMatch, [&state, &currentNode](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingDefinition)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected ICON at line %d"), lineNum));
		}
		if (match.size() > 1 && match[1].matched)
		{
			currentNode.IconType = FindEnumValueByName<EConfiguratorNodeIconType>(TEXT("EConfiguratorNodeIconType"), match[1].str().c_str());
		}
		else
		{
			errorReporter(*FString::Printf(TEXT("Badly formed ICON at line %d"), lineNum));
		}

		TArray<FString> suffices;
		FString(match.suffix().str().c_str()).ParseIntoArray(suffices, TEXT(" "));
		for (auto &suffix : suffices)
		{
			EConfiguratorNodeIconOrientation orientation;
			if (TryFindEnumValueByName<EConfiguratorNodeIconOrientation>(TEXT("EConfiguratorNodeIconOrientation"), *suffix, orientation))
			{
				currentNode.Orientation = orientation;
			}
			else if (suffix == TEXT("CanSwitch"))
			{
				currentNode.CanFlipOrientation = true;
			}
			else
			{
				errorReporter(*FString::Printf(TEXT("Unrecognized ICON directive %s at line %d"), *suffix, lineNum));
			}
		}
	},
		true);

	static const FString kQuotedString = TEXT("\"([\\w\\s\\-\\.\\,\"/:]+)\"");
	static const FString kPrimitiveTypeMatch = TEXT("(STRING|DIMENSION|PRIVATE|BOOLEAN|FIXED)");

	//Match PROPERTY definition (ie STRING("Name",Node.Name))
	static const FString kPropertyPattern = kPrimitiveTypeMatch + TEXT("\\(") + kQuotedString + TEXT("\\,(") + kPropertyMatch + TEXT(")\\)");
	compiler.AddRule(kPropertyPattern, [&state, &currentNode](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingProperties)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected PROPERTY spec at line %d"), lineNum));
		}
		FValueSpec vs(match[3].str().c_str());
		FString type = match[1].str().c_str();
		if (type != TEXT("PRIVATE"))
		{
			currentNode.FormItemToProperty.Add(match[2].str().c_str(), vs.QN());
		}
		currentNode.Properties.SetProperty(vs.Scope, vs.Name, TEXT(""));
	},
		false);

	// Match BIND(Scope.Property,OtherScope.OtherProperty)
	// Used to bind properties like Layer.Thickness to child values like Node.Depth
	static const FString kBindPattern = TEXT("BIND\\((") + kPropertyMatch + TEXT(")\\,(") + kPropertyMatch + TEXT(")\\)");
	compiler.AddRule(kBindPattern, [&state, &currentNode](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingProperties)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected BIND spec at line %d"), lineNum));
		}

		FValueSpec source(match[1].str().c_str());
		FValueSpec target(match[2].str().c_str());

		if (!currentNode.Properties.BindProperty(source.Scope, source.Name, target.Scope, target.Name))
		{
			errorReporter(*FString::Printf(TEXT("Could not bind property at %d"), lineNum));
		}

	},
		false);

	static const FString kNodePinTagMatch = TEXT("(") + kSimpleWord + TEXT(")=(") + kSimpleWord + TEXT(")\\(") + kTagListRequired + TEXT("\\)\\.(") + kSimpleWord + TEXT(")");

	compiler.AddRule(kNodePinTagMatch, [this, &state, &currentNode, &currentPreset](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingPreset)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected list assignment at line %d"), lineNum));
			return;
		}

		FName nodeTarget = match[1].str().c_str();

		FCraftingTreeNodeType *parentNode = NodeDescriptors.Find(currentPreset.NodeType);
		if (parentNode == nullptr)
		{
			errorReporter(*FString::Printf(TEXT("Unidentified preset node type %s line %d"), *currentPreset.NodeType.ToString(), lineNum));
			return;
		}

		FName pinNodeType = match[2].str().c_str();

		if (NodeDescriptors.Find(pinNodeType) == nullptr)
		{
			errorReporter(*FString::Printf(TEXT("Unidentified preset node type %s line %d"), *pinNodeType.ToString(), lineNum));
			return;
		}

		FString tagsStr = match[3].str().c_str();
		TArray<FString> tagStrs;
		tagsStr.ParseIntoArray(tagStrs, TEXT(","));

		FName selectedPreset = match[5].str().c_str();

		bool foundSet = false;
		for (int32 i = 0; i < parentNode->InputPins.Num(); ++i)
		{
			if (parentNode->InputPins[i].SetName == nodeTarget)
			{
				//Don't ChildNodes.AddDefaulted_GetRef because we iterate over ChildNodes below
				FCraftingTreeNodePreset::FPinSpec newSpec;
				newSpec.PinSetIndex = i;
				newSpec.PinSetPosition = 0;
				newSpec.Scope = parentNode->InputPins[i].Scope;

				FCraftingPresetSearchSelection &child = newSpec.PresetSequence.AddDefaulted_GetRef();
				child.NodeType = pinNodeType;
				child.SelectedPresetID = selectedPreset;

				for (auto &tag : tagStrs)
				{
					child.SelectionSearchTags.AddUnique(tag);
				}

				for (auto &pin : currentPreset.ChildNodes)
				{
					if (pin.PinSetIndex == i && pin.PinSetPosition >= newSpec.PinSetPosition)
					{
						newSpec.PinSetPosition = pin.PinSetPosition + 1;
					}
				}

				currentPreset.ChildNodes.Add(newSpec);

				foundSet = true;
				break;
			}
		}

		if (!foundSet)
		{
			errorReporter(*FString::Printf(TEXT("Unidentified pin tag target %s line %d"), *nodeTarget.ToString(), lineNum));
		}
	}, false);

	static const FString kRangeMatch = TEXT("\\[(") + kNumberMatch + TEXT(")") + kElipsesMatch + TEXT("(") + kNumberMatch + TEXT(")\\]");
	static const FString kOpenRangeMatch = TEXT("\\[(") + kNumberMatch + TEXT(")") + kElipsesMatch + TEXT("\\]");
	static const FString kScopeMatch = TEXT("(") + kSimpleWord + TEXT(")->");
	static const FString kNodePinMatch = kScopeMatch + TEXT("(") + kSimpleWord + TEXT(")") + TEXT("(") + kRangeMatch + TEXT("|") + kOpenRangeMatch + TEXT(")*");

	//Match pin definition (ie Layer->MODULE[1..])
	compiler.AddRule(kNodePinMatch, [this, &state, &currentNode](
		const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingPins)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected INPUT spec at line %d"), lineNum));
		}
		if (match.size() < 7)
		{
			errorReporter(*FString::Printf(TEXT("Malformed INPUT spec at line %d"), lineNum));
		}
		else
		{
			FCraftingTreeNodePinSet &pinset = currentNode.InputPins.AddDefaulted_GetRef();

			if (!TryFindEnumValueByName<EBIMValueScope>(TEXT("EBIMValueScope"), match[1].str().c_str(), pinset.Scope))
			{
				errorReporter(*FString::Printf(TEXT("Unidentified scope %s at line %d"), match[1].str().c_str(), lineNum));
			}

			pinset.SetName = match[2].str().c_str();

			//fully qualified range (ie [1..3])
			if (match[5].matched)
			{
				pinset.MinCount = FCString::Atoi(match[4].str().c_str());
				pinset.MaxCount = FCString::Atoi(match[5].str().c_str());
			}
			//open-ended range (ie [1..])
			else if (match[6].matched)
			{
				pinset.MinCount = FCString::Atoi(match[6].str().c_str());
				pinset.MaxCount = -1;
			}
			//no range specified, only one pin
			else
			{
				pinset.MinCount = pinset.MaxCount = 1;
			}
		}
	},
		false);

	static const FString kTags = TEXT("TAGS=\\[");
	static const FString kTagMatch = kTags + kTagListRequired + TEXT("\\]");

	compiler.AddRule(kTagMatch, [this, &state, &currentPreset](const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingPreset)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected TAGS at line %d"), lineNum));
		}
		else
		{
			FString tagStr = match[1].str().c_str();
			TArray<FString> elements;
			tagStr.ParseIntoArray(elements, TEXT(","));
			for (auto &element : elements)
			{
				TArray<FString> tag;
				element.ParseIntoArray(tag, TEXT("."));
				if (tag.Num() != 2)
				{
					errorReporter(*FString::Printf(TEXT("Badly formed tag %s at line %d"), *element, lineNum));
				}
				else
				{
					TArray<FCraftingPresetTag> &tagArray = currentPreset.Tags.FindOrAdd(*tag[0]);
					tagArray.Add(FCraftingPresetTag(*tag[0], *tag[1]));
				}
			}
		}
	}, false);

	static const FString kNodeInstanceDecl = TEXT("(") + kNodeName + TEXT(")") + kWhitespace + TEXT("(") + kSimpleWord + TEXT(")");
	static const FString kPreset = TEXT("PRESET");
	static const FString kPresetMatch = kPreset + kWhitespace + kNodeInstanceDecl;
	static const FString kReadOnlyFlag = TEXT("READONLY");

	compiler.AddRule(kPresetMatch, [this, &state, &currentPreset](const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != Neutral)
		{
			// No need to bail, but log the error
			errorReporter(*FString::Printf(TEXT("Unexpected PRESET definition at line %d"), lineNum));
		}

		if (match.size() < 3)
		{
			errorReporter(*FString::Printf(TEXT("Malformed PRESET definition at line %d"), lineNum));
			return;
		}

		state = ReadingPreset;

		currentPreset = FCraftingTreeNodePreset();

		currentPreset.PresetID = match[2].str().c_str();
		if (Presets.Contains(currentPreset.PresetID))
		{
			errorReporter(*FString::Printf(TEXT("PRESET %s redefined at line %d"), *currentPreset.PresetID.ToString(), lineNum));
			return;
		}

		currentPreset.NodeType = match[1].str().c_str();
		FCraftingTreeNodeType *baseDescriptor = NodeDescriptors.Find(currentPreset.NodeType);

		if (baseDescriptor == nullptr)
		{
			errorReporter(*FString::Printf(TEXT("Unrecognized PRESET type %s at line %d"), *currentPreset.NodeType.ToString(), lineNum));
			return;
		}

		currentPreset.Properties = baseDescriptor->Properties;

		TArray<FString> suffices;
		FString(match.suffix().str().c_str()).ParseIntoArray(suffices, TEXT(" "));
		for (auto &suffix : suffices)
		{
			if (suffix == kReadOnlyFlag)
			{
				currentPreset.IsReadOnly = true; // set to false by default in the class definition constructor
			}
		}

	}, true); // true here indicates that presets are allowed to have a suffix after the main match, used for READONLY and other flags

	static const FString kVariableAssignment = TEXT("(") + kVariable + TEXT(")=") + kQuotedString;
	compiler.AddRule(kVariableAssignment, [this, &state, &currentPreset](const TCHAR *originalLine,
		const FModumateScriptProcessor::TRegularExpressionMatch &match,
		int32 lineNum,
		FModumateScriptProcessor::TErrorReporter errorReporter)
	{
		if (state != ReadingPreset)
		{
			errorReporter(*FString::Printf(TEXT("Unexpected variable assignment at line %d"), lineNum));
			return;
		}
		if (match.size() < 3)
		{
			errorReporter(*FString::Printf(TEXT("Badly formed variable assignment at line %d"), lineNum));
		}

		FValueSpec vs(match[1].str().c_str());
		currentPreset.Properties.SetProperty(vs.Scope, vs.Name, match[2].str().c_str());

		return;
	}, false);

	ECraftingResult result = compiler.ParseFile(FilePath, [&OutMessages](const FString &msg) {OutMessages.Add(msg); }) ? ECraftingResult::Success : ECraftingResult::Error;

	/*
	Presets may be tagged after they are attached to previously defined pins, so gather them after read and update pins
	*/

	return result;
}}}