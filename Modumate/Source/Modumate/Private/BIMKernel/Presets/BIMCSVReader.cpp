// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMCSVReader.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Database/ModumateObjectEnums.h"

FBIMCSVReader::FBIMCSVReader()
{
}

bool FBIMCSVReader::FPresetMatrix::IsIn(int32 i) const
{
	return First >= 0 && i >= First && i < First + Columns.Num();
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

EBIMResult FBIMCSVReader::ProcessNodeTypeRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[TYPENAME][][Name][][Scope][][Property Class]
	// where Scope is the scope for all named properties (using to be deprecated property system) 
	// and Property Class is the class name of the new uproperty sheet (which will have inherent scope)

	NodeType.TypeName = Row[2];

	if (NodeType.TypeName.IsNone())
	{
		return EBIMResult::Error;
	}

	FName scope = Row[4];
	if (scope.IsNone())
	{
		return EBIMResult::Error;
	}
	
	NodeType.Scope = BIMValueScopeFromName(scope);

	if (NodeType.Scope == EBIMValueScope::Error || NodeType.Scope == EBIMValueScope::None)
	{
		return EBIMResult::Error;
	}

	return EBIMResult::Success;
}

EBIMResult FBIMCSVReader::ProcessInputPinRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[INPUTPIN][][extensibility][][set name] where extensibility is min..max for the number of children supported

	FString extensibility = Row[2];

	if (extensibility.IsEmpty())
	{
		return EBIMResult::Error;
	}
	FBIMPresetNodePinSet& childAttachment = NodeType.PinSets.AddDefaulted_GetRef();

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

	return EBIMResult::Success;
}

EBIMResult FBIMCSVReader::ProcessTagPathRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
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

	int32 currentRange = INDEX_NONE;

	PresetMatrices.Empty();

	for (int32 i = 1; i < Row.Num(); ++i)
	{
		FString cell = Row[i];

		if (cell.IsEmpty())
		{
			continue;
		}

		TArray<FString> dataType;

		//When we encounter a new DataType column, set up the corresponding state
		if (cell.ParseIntoArray(dataType, TEXT("=")) && dataType[0].Equals(TEXT("DataType")))
		{
			ECSVMatrixNames matrixName = FindEnumValueByName<ECSVMatrixNames>(TEXT("ECSVMatrixNames"), *dataType[1]);
			if (matrixName != ECSVMatrixNames::Error)
			{
				PresetMatrices.Add(FPresetMatrix(matrixName, i + 1));
				currentRange = PresetMatrices.Num() - 1;
			}
			else
			{
				currentRange = INDEX_NONE;
			}
			continue;
		}

		if (currentRange != INDEX_NONE)
		{
			FPresetMatrix& currentMatrix = PresetMatrices[currentRange];
			FString normalizedCell = NormalizeCell(cell);
			currentMatrix.Columns.Add(normalizedCell);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMCSVReader::ProcessPresetRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TMap<FBIMKey, FBIMPresetInstance>& OutPresets, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages)
{
	//Row Format:
	//[Preset]([]([column data]+))*

	/*
	The columns in a preset correspond to the definitions stored in the various column range object
	For each column, find the range it lands into and set a corresponding value (property, pin, slot, etc)
	*/

	for (auto& presetMatrix : PresetMatrices)
	{
		switch (presetMatrix.Name)
		{
			case ECSVMatrixNames::ID:
			{
				FBIMKey presetID = FBIMKey(NormalizeCell(Row[presetMatrix.First]));
				if (!presetID.IsNone())
				{
					if (!Preset.PresetID.IsNone())
					{
						Preset.TryGetProperty(BIMPropertyNames::Name, Preset.DisplayName);
						OutPresets.Add(Preset.PresetID, Preset);
						Preset = FBIMPresetInstance();
					}

					Preset.PresetID = presetID;
					Preset.NodeType = NodeType.TypeName;
					Preset.NodeScope = NodeType.Scope;
					Preset.TypeDefinition = NodeType;
				}
				ensureAlways(!Preset.PresetID.IsNone());
			}
			break;

			case ECSVMatrixNames::GUID :
			{
				FString guidStr(Row[presetMatrix.First]);
				if (!guidStr.IsEmpty())
				{
					Preset.GUID = FBIMKey(guidStr);
				}
			}
			break;

			case ECSVMatrixNames::Profile:
			{
				FString profileKey(Row[presetMatrix.First]);
				if (!profileKey.IsEmpty())
				{
					Preset.Properties.SetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID, NormalizeCell(profileKey));
				}
			}
			break;

			case ECSVMatrixNames::Material :
			{
				// TODO: Rows 0, 2 & 3 are channel, surface material and color tint, to be implemented later
				FString rawMaterial(Row[presetMatrix.First + 1]);
				FString hexValue(Row[presetMatrix.First + 4]);
				if (!rawMaterial.IsEmpty())
				{
					Preset.Properties.SetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, NormalizeCell(rawMaterial));
				}
				if (!hexValue.IsEmpty())
				{
					Preset.Properties.SetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, hexValue);
				}
			}
			break;

			case ECSVMatrixNames::Dimensions : 
			{
				FModumateFormattedDimension measurement = UModumateDimensionStatics::StringToFormattedDimension(Row[presetMatrix.First + 1]);
				Preset.Properties.SetProperty(EBIMValueScope::Dimension, FName(Row[presetMatrix.First]), measurement.Centimeters);
			}
			break;

			case ECSVMatrixNames::StartsInProject:
			{
				ensureAlways(!Preset.PresetID.IsNone());
				FString cellStr = Row[presetMatrix.First];
				if (!cellStr.IsEmpty())
				{
					OutStarters.Add(Preset.PresetID);
				}
			}
			break;

			case ECSVMatrixNames::Properties:
			{
				ensureAlways(!Preset.PresetID.IsNone());
				for (int32 i = 0; i < presetMatrix.Columns.Num(); ++i)
				{
					const FString& column = presetMatrix.Columns[i];

					FBIMPropertyKey propSpec(*column);
					FString cell = Row[presetMatrix.First + i];

					// Only add blank properties if they don't already exist
					if (!Preset.HasProperty(propSpec.Name) || !cell.IsEmpty())
					{
						EBIMValueType* propType = PropertyTypeMap.Find(propSpec.QN());
						if (ensureAlways(propType != nullptr))
						{
							switch (*propType)
							{
								// These values are stored as is from the spreadsheet
							case EBIMValueType::Vector:
							case EBIMValueType::DimensionSet:
							case EBIMValueType::String:
							case EBIMValueType::HexValue:
							case EBIMValueType::Formula:
							case EBIMValueType::Boolean:
							case EBIMValueType::DisplayText:
							{
								Preset.SetScopedProperty(propSpec.Scope, propSpec.Name, cell);
							}
							break;

							// These values want to have whitespace stripped
							case EBIMValueType::PresetID:
							case EBIMValueType::AssetPath:
							case EBIMValueType::CategoryPath:
							{
								Preset.SetScopedProperty(propSpec.Scope, propSpec.Name, NormalizeCell(cell));
							}
							break;

							case EBIMValueType::Dimension:
							{
								Preset.SetScopedProperty(propSpec.Scope, propSpec.Name, UModumateDimensionStatics::StringToFormattedDimension(cell).Centimeters);
							}
							break;

							case EBIMValueType::Number:
							{
								Preset.SetScopedProperty(propSpec.Scope, propSpec.Name, FCString::Atof(*cell));
							}
							break;

							default:
								ensureAlways(false);
							};
						}
					}
				}
			}
			break;

			case ECSVMatrixNames::MyCategoryPath:
			{
				ensureAlways(!Preset.PresetID.IsNone());
				FString ncp = Row[presetMatrix.First];
				if (!ncp.IsEmpty())
				{
					Preset.MyTagPath.FromString(Row[presetMatrix.First]);
				}
			}
			break;

			case ECSVMatrixNames::ParentCategoryPath:
			{
				ensureAlways(!Preset.PresetID.IsNone());
				for (int32 i = 0; i < presetMatrix.Columns.Num(); ++i)
				{
					FString cell = Row[presetMatrix.First + i];
					if (!cell.IsEmpty())
					{
						FBIMTagPath& path = Preset.ParentTagPaths.AddDefaulted_GetRef();
						if (cell.Equals(TEXT("X")))
						{
							path.FromString(presetMatrix.Columns[i]);
						}
						else
						{
							path.FromString(NormalizeCell(cell));
						}
					}
				}
			}
			break;

			case ECSVMatrixNames::Slots:
			{
				ensureAlways(!Preset.PresetID.IsNone());
				for (int32 i = 0; i < presetMatrix.Columns.Num(); ++i)
				{
					FString category = presetMatrix.Columns[i];
					FString cell = Row[presetMatrix.First + i];
					if (category.Equals(TEXT("SlotConfig")))
					{
						if (!cell.IsEmpty())
						{
							Preset.SlotConfigPresetID = FBIMKey(NormalizeCell(*cell));
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
					else if (category.Equals(TEXT("SlotName")))
					{
						if (Preset.PartSlots.Num() == 0 || !Preset.PartSlots.Last().SlotPreset.IsNone())
						{
							Preset.PartSlots.AddDefaulted();
						}
						Preset.PartSlots.Last().SlotPreset = FBIMKey(*cell);
					}
				}
			}
			break;

			case ECSVMatrixNames::InputPins:
			{
				ensureAlways(!Preset.PresetID.IsNone());
				for (int32 i = 0; i < presetMatrix.Columns.Num(); ++i)
				{
					FString cell = Row[presetMatrix.First + i];
					if (cell.IsEmpty())
					{
						continue;
					}
					FName pinName = *presetMatrix.Columns[i];
					bool found = false;
					for (int32 setIndex = 0; setIndex < NodeType.PinSets.Num(); ++setIndex)
					{
						if (NodeType.PinSets[setIndex].SetName == pinName)
						{
							int32 setPosition = 0;
							for (auto& cap : Preset.ChildPresets)
							{
								if (cap.ParentPinSetIndex == setIndex)
								{
									setPosition = FMath::Max(cap.ParentPinSetPosition + 1, setPosition);
								}
							}

							FBIMPresetPinAttachment& newCAP = Preset.ChildPresets.AddDefaulted_GetRef();
							newCAP.ParentPinSetIndex = setIndex;
							newCAP.ParentPinSetPosition = setPosition;
							newCAP.Target = NodeType.PinSets[setIndex].PinTarget;

							newCAP.PresetID = FBIMKey(NormalizeCell(cell));
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
						OutMessages.Add(FString::Printf(TEXT("Could not find pin %s"), *presetMatrix.Columns[i]));
					}
				}
			}
			break;

			case ECSVMatrixNames::MaterialChannels:
				{
					ensureAlways(!Preset.PresetID.IsNone());
					// Pin channels are used to assign materials to meshes
					// Materials are siblings of their meshes, so we tag all siblings with the pin channel to link them in FBimAssemblSpec::FromPreset
					if (ensureAlways(Preset.ChildPresets.Num() > 0))
					{
						int32 channelPosition = Preset.ChildPresets.Last().ParentPinSetPosition;
						for (auto& cp : Preset.ChildPresets)
						{
							if (cp.ParentPinSetPosition == channelPosition)
							{
								cp.PinChannel = Row[presetMatrix.First];
							}
						}
					}
				}
				break;
		}
	}

	return EBIMResult::Success;
}

EBIMResult FBIMCSVReader::ProcessPropertyDeclarationRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[PROPERTY][][Property Type][][Property Name][][Property Value]
	FName propertyTypeName = Row[2];
	FString displayName = Row[4];
	EBIMValueType propertyTypeEnum;
	FBIMPropertyKey qualifiedName = Row[6];

	if (!TryFindEnumValueByName<EBIMValueType>(TEXT("EBIMValueType"), propertyTypeName, propertyTypeEnum))
	{
		return EBIMResult::Error;
	}

	PropertyTypeMap.Add(qualifiedName.QN(), propertyTypeEnum);

	if (qualifiedName.Scope == EBIMValueScope::None)
	{
		return EBIMResult::Error;
	}
	else
	{
		NodeType.Properties.SetProperty(qualifiedName.Scope, qualifiedName.Name, FString());

		//Properties are only visible in the editor if they nave display names
		if (!displayName.IsEmpty())
		{
			NodeType.FormItemToProperty.Add(displayName, qualifiedName.QN());
		}
	}
	return EBIMResult::Success;
}