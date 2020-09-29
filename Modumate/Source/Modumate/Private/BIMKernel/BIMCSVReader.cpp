// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMCSVReader.h"

FBIMCSVReader::FBIMCSVReader()
{
	PropertyFactoryMap.Add(UBIMPropertiesDIM::StaticClass()->GetFName(), []() {return NewObject<UBIMPropertiesDIM>(); });
	PropertyFactoryMap.Add(UBIMPropertiesSlot::StaticClass()->GetFName(), []() {return NewObject<UBIMPropertiesSlot>(); });

	ParentPathRange.wantTags = true;
}

bool FBIMCSVReader::FColumnRange::IsIn(int32 i) const 
{
	return first >= 0 && i >= first && i < first + columns.Num();
}

const FString& FBIMCSVReader::FColumnRange::Get(int32 i) const
{
	return columns[i - first];
}

static FString NormalizeCell(const FString& Row)
{
	int32 sl = Row.Len();

	FString cell;
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

ECraftingResult FBIMCSVReader::ProcessNodeTypeRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[TYPENAME][][Name][][Scope][][Property Class]
	// where Scope is the scope for all named properties (using to be deprecated property system) 
	// and Property Class is the class name of the new uproperty sheet (which will have inherent scope)

	NodeType.TypeName = Row[2];

	if (NodeType.TypeName.IsNone())
	{
		return ECraftingResult::Error;
	}

	FName scope = Row[4];
	if (scope.IsNone())
	{
		return ECraftingResult::Error;
	}
	
	NodeType.Scope = BIMValueScopeFromName(scope);

	PropertyClass = Row[6];

	if (NodeType.Scope == EBIMValueScope::Error || NodeType.Scope == EBIMValueScope::None)
	{
		return ECraftingResult::Error;
	}

	return ECraftingResult::Success;
}

ECraftingResult FBIMCSVReader::ProcessInputPinRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[INPUTPIN][][extensibility][][set name] where extensibility is min..max for the number of children supported

	FString extensibility = Row[2];

	if (extensibility.IsEmpty())
	{
		return ECraftingResult::Error;
	}
	FChildAttachmentType &childAttachment = NodeType.ChildAttachments.AddDefaulted_GetRef();

	childAttachment.SetName = Row[4];

	TArray<FString> range;

	//min..max indicates a closed range of children (ie 1 to 4) and min.. indicates an open range (ie 2 or more)
	FString(extensibility).ParseIntoArray(range, TEXT(".."));
	if (range.Num() == 2)
	{
		childAttachment.MinCount = FCString::Atoi(*range[0]);
		childAttachment.MaxCount = FCString::Atoi(*range[1]);
	}
	else if (range.Num() == 1)
	{
		childAttachment.MinCount = FCString::Atoi(*range[0]);
		childAttachment.MaxCount = -1; // -1 indicates no maximum...TODO: make a named constant
	}

	FName target = Row[6];
	
	EBIMPinTarget targetEnum = FindEnumValueByName<EBIMPinTarget>(TEXT("EBIMPinTarget"), target);
	if (targetEnum != EBIMPinTarget::None)
	{
		childAttachment.PinTarget = targetEnum;
	}
	else
	{
		childAttachment.PinTarget = EBIMPinTarget::Default;
	}

	return ECraftingResult::Success;
}

ECraftingResult FBIMCSVReader::ProcessTagPathRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[TAGPATHS][DataType=ID][ID]([DataType=?][..])*

	/*
	The Tag path row defines not only tag paths, but column ranges for the different kinds of data a preset will need
	Column ranges are delimited by a 'DataType=X' column, where X indicates a data category
	When presets are read using the PRESET row handler, columns numbers are checked against these DataType entries 
	  to determine how a given cell should be interpreted
	*/

	//The different kinds of data columns we may be reading

	FColumnRange* currentRange = nullptr;

	auto setColumnRange = [this, &currentRange](FColumnRange &ColumnRange, int32 StartingColumn)
	{
		currentRange = &ColumnRange;
		ColumnRange.first = StartingColumn;
	};

	for (int32 i = 1; i < Row.Num(); ++i)
	{
		FString cell = Row[i];

		if (cell.IsEmpty())
		{
			continue;
		}

		FString normalizedCell = NormalizeCell(cell);
		TArray<FString> dataType;

		//When we encounter a new DataType column, set up the corresponding state
		if (cell.ParseIntoArray(dataType, TEXT("=")) && dataType[0].Equals(TEXT("DataType")))
		{
			if (dataType[1].Equals(TEXT("Slots")))
			{
				setColumnRange(SlotRange, i + 1);
			}
			else if (dataType[1].Equals(TEXT("Properties")))
			{
				setColumnRange(PropertyRange, i + 1);
			}
			else if (dataType[1].Equals(TEXT("StartsInProject")))
			{
				setColumnRange(StartInProjectRange, i + 1);
			}
			else if (dataType[1].Equals(TEXT("InputPins")))
			{
				setColumnRange(PinRange, i + 1);
			}
			else if (dataType[1].Contains(TEXT("Parent")))
			{
				setColumnRange(ParentPathRange, i + 1);
			}
			else if (dataType[1].Contains(TEXT("MyNode")))
			{
				setColumnRange(MyPathRange, i + 1);
			}
			else if (dataType[1].Equals(TEXT("ID")))
			{
				setColumnRange(IDRange, i + 1);
			}
			else
			{
				currentRange = nullptr;
			}
			continue;
		}

		if (currentRange != nullptr)
		{
			currentRange->columns.Add(normalizedCell);
			if (currentRange->wantTags)
			{
				FBIMTagPath path;
				path.FromString(normalizedCell);
				currentRange->columnTags.Add(path);
			}
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMCSVReader::ProcessPresetRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TMap<FBIMKey, FBIMPreset>& OutPresets, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages)
{
	//Row Format:
	//[Preset]([]([column data]+))*

	/*
	The columns in a preset correspond to the definitions stored in the various column range object
	For each column, find the range it lands into and set a corresponding value (property, pin, slot, etc)
	*/
	for (int32 i = 1; i < Row.Num(); ++i)
	{
		FString cell = NormalizeCell(Row[i]);

		if (IDRange.IsIn(i))
		{
			if (!cell.IsEmpty())
			{
				if (!Preset.PresetID.IsNone())
				{
					OutPresets.Add(Preset.PresetID, Preset);
					Preset = FBIMPreset();
				}

				if (UPropertySheet != nullptr)
				{
					UPropertySheet->RemoveFromRoot();
				}

				Preset.PresetID = FBIMKey(*cell);
				Preset.NodeType = NodeType.TypeName;
				Preset.NodeScope = NodeType.Scope;

				// TODO: UPropertySheet to be added to a preset UPROPERTY, meantime don't actually make any unless testing
#ifdef TEST_BIM_UPROPERTIES
				if (!PropertyClass.IsNone())
				{
					auto* factoryFunc = PropertyFactoryMap.Find(PropertyClass);
					if (ensureAlways(factoryFunc != nullptr))
					{
						UPropertySheet = (*factoryFunc)();
						if (ensureAlways(UPropertySheet != nullptr))
						{
							UPropertySheet->AddToRoot();
						}
					}
					else
					{
						OutMessages.Add(FString::Printf(TEXT("Unidentified BIM Property Type %s"), Row[6]));
						return ECraftingResult::Error;
					}
				}
#endif
			}
		}
		else if (StartInProjectRange.IsIn(i))
		{
			if (!cell.IsEmpty())
			{
				OutStarters.Add(Preset.PresetID);
			}
		}
		else if (PropertyRange.IsIn(i))
		{
			FBIMPropertyValue propSpec(*PropertyRange.Get(i));
			// Only add blank properties if they don't already exist
			if (!Preset.HasProperty(propSpec.Name) || !cell.IsEmpty())
			{
				Preset.SetScopedProperty(propSpec.Scope, propSpec.Name, cell);
			}

			if (UPropertySheet != nullptr)
			{
				EBIMValueType* valueType = PropertyTypeMap.Find(propSpec.Name);

				if (valueType == nullptr)
				{
					return ECraftingResult::Error;
				}

				switch (*valueType)
				{
				case EBIMValueType::Formula:
				case EBIMValueType::String:
					UPropertySheet->SetStringProperty(propSpec.Name, cell);
					break;

				case EBIMValueType::Dimension:
				case EBIMValueType::Angle:
				{
					if (!cell.IsEmpty())
					{
						UPropertySheet->SetFloatProperty(propSpec.Name, FCString::Atof(*cell));
					}
				}
					break;
				default:
					return ECraftingResult::Error;

				};
			}
		}
		else if (MyPathRange.IsIn(i) && !cell.IsEmpty())
		{
			Preset.MyTagPath.FromString(cell);
		}
		else if (ParentPathRange.IsIn(i) && !cell.IsEmpty())
		{
			FBIMTagPath &path = Preset.ParentTagPaths.AddDefaulted_GetRef();
			if (cell.Equals(TEXT("X")))
			{
				path.FromString(ParentPathRange.Get(i));
			}
			else
			{
				path.FromString(NormalizeCell(cell));
			}
		}
		else if (SlotRange.IsIn(i))
		{
			FString category = SlotRange.Get(i);
			if (category.Equals(TEXT("SlotConfig")))
			{
				if (!cell.IsEmpty())
				{
					Preset.SlotConfigPresetID = FBIMKey(*NormalizeCell(cell));
				}
			}
			else if (category.Equals(TEXT("PartPreset")) && ensureAlways(!cell.IsEmpty()))
			{
				if (Preset.PartSlots.Num() == 0 || !Preset.PartSlots.Last().PartPreset.IsNone())
				{
					Preset.PartSlots.AddDefaulted();
				}
				Preset.PartSlots.Last().PartPreset = FBIMKey(*NormalizeCell(cell));
			}
			else if (category.Equals(TEXT("ID")))
			{
				if (Preset.PartSlots.Num() == 0 || !Preset.PartSlots.Last().ID.IsNone())
				{
					Preset.PartSlots.AddDefaulted();
				}
				Preset.PartSlots.Last().ID = FBIMKey(*NormalizeCell(cell));
			}
			else if (category.Equals(TEXT("ParentID")))
			{
				if (Preset.PartSlots.Num() == 0 || !Preset.PartSlots.Last().ParentID.IsNone())
				{
					Preset.PartSlots.AddDefaulted();
				}
				Preset.PartSlots.Last().ParentID = FBIMKey(*NormalizeCell(cell));
			}
			else if (category.Equals(TEXT("SlotID")))
			{
				if (Preset.PartSlots.Num() == 0 || !Preset.PartSlots.Last().SlotName.IsNone())
				{
					Preset.PartSlots.AddDefaulted();
				}
				Preset.PartSlots.Last().SlotName = FBIMKey(*NormalizeCell(cell));
			}
		}
		else if (PinRange.IsIn(i))
		{
			if (cell.IsEmpty())
			{
				continue;
			}
			FName pinName = *PinRange.Get(i);
			bool found = false;
			for (int32 setIndex = 0; setIndex < NodeType.ChildAttachments.Num(); ++setIndex)
			{
				if (NodeType.ChildAttachments[setIndex].SetName == pinName)
				{
					int32 setPosition = 0;
					for (auto &cap : Preset.ChildPresets)
					{
						if (cap.ParentPinSetIndex == setIndex)
						{
							setPosition = FMath::Max(cap.ParentPinSetPosition + 1, setPosition);
						}
					}
					FBIMPreset::FChildAttachment &newCAP = Preset.ChildPresets.AddDefaulted_GetRef();
					newCAP.ParentPinSetIndex = setIndex;
					newCAP.ParentPinSetPosition = setPosition;
					newCAP.Target = NodeType.ChildAttachments[setIndex].PinTarget;

					FString rowStr = cell;
					rowStr.RemoveSpacesInline();
					newCAP.PresetID = FBIMKey(*rowStr);
					if (newCAP.PresetID.IsNone() && Preset.ChildPresets.Num() > 1)
					{
						newCAP.PresetID = Preset.ChildPresets[0].PresetID;
					}
					ensureAlways(!newCAP.PresetID.IsNone());
					found = true;
				}
			}

			if (!found)
			{
				OutMessages.Add(FString::Printf(TEXT("Could not find pin %s"), *PinRange.Get(i)));
			}
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMCSVReader::ProcessPropertyDeclarationRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[PROPERTY][][Property Type][][Property Name][][Property Value]
	FName propertyTypeName = Row[2];
	FString propertyName = Row[4];
	EBIMValueType propertyTypeEnum;
	FBIMPropertyValue propertyValue = Row[6];

	if (!TryFindEnumValueByName<EBIMValueType>(TEXT("EBIMValueType"), propertyTypeName, propertyTypeEnum))
	{
		return ECraftingResult::Error;
	}

	PropertyTypeMap.Add(*propertyName, propertyTypeEnum);

	if (propertyValue.Scope == EBIMValueScope::None)
	{
		return ECraftingResult::Error;
	}
	else
	{
		NodeType.Properties.SetProperty(propertyValue.Scope, propertyValue.Name, FBIMPropertyValue::FValue());

		//Properties are only visible in the editor if they nave display names
		if (!propertyName.IsEmpty())
		{
			NodeType.FormItemToProperty.Add(propertyName, propertyValue.QN());
		}
	}
	return ECraftingResult::Success;
}