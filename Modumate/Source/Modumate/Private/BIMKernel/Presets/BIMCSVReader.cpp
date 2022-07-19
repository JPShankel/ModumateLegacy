// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMCSVReader.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/EdgeDetailData.h"
#include "Objects/ModumateObjectEnums.h"
#include "Quantities/QuantitiesDimensions.h"
#include "BIMKernel/Presets/BIMPresetPatternDefinition.h"
#include "ModumateCore/EnumHelpers.h"

TMap<FBIMKey, FGuid> FBIMCSVReader::KeyGuidMap;
TMap<FGuid, FBIMKey> FBIMCSVReader::GuidKeyMap;

#define LOCTEXT_NAMESPACE "BIMCSV"

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
	
	EBIMPinTarget targetEnum = EBIMPinTarget::None;
	if (FindEnumValueByName(target, targetEnum))
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

	// All presets have marks and comments
	FBIMPropertyKey propertyKey(EBIMValueScope::Preset, BIMPropertyNames::Mark);
	NodeType.Properties.SetProperty<FString>(propertyKey.Scope, propertyKey.Name, FString());
	NodeType.FormTemplate.AddPropertyElement(LOCTEXT("BIMMark","Mark"), propertyKey.QN(), EBIMPresetEditorField::TextProperty);

	propertyKey = FBIMPropertyKey(EBIMValueScope::Preset, BIMPropertyNames::Comments);
	NodeType.Properties.SetProperty<FString>(propertyKey.Scope, propertyKey.Name, FString());
	NodeType.FormTemplate.AddPropertyElement(LOCTEXT("BIMComments", "Comments"), propertyKey.QN(), EBIMPresetEditorField::TextProperty);

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
			ECSVMatrixNames matrixName = ECSVMatrixNames::Error;
			if (FindEnumValueByName(*dataType[1], matrixName))
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

EBIMResult FBIMCSVReader::ProcessPresetRow(const TArray<const TCHAR*>& Row, int32 RowNumber, FBIMPresetCollection& OutPresets, TArray<FGuid>& OutStarters, TArray<FString>& OutMessages)
{
	//Row Format:
	//[Preset]([]([column data]+))*

	/*
	The columns in a preset correspond to the definitions stored in the various column range object
	For each column, find the range it lands into and set a corresponding value (property, pin, slot, etc)
	*/

	// Handle ID first
	// If there is no ID, this is a multi-line preset
	// If there is an ID and a pre-existing GUID, we just wrapped up
	bool bOnFirstRow = false;
	for (auto& presetMatrix : PresetMatrices)
	{
		if (presetMatrix.Name == ECSVMatrixNames::ID)
		{
			FBIMKey presetID = FBIMKey(NormalizeCell(Row[presetMatrix.First]));

			if (!presetID.IsNone())
			{
				bOnFirstRow = true;
				if (!Preset.PresetID.IsNone())
				{
					if (OutPresets.PresetFromGUID(Preset.GUID)==nullptr)
					{
						Preset.TryGetProperty(BIMPropertyNames::Name, Preset.DisplayName);
						OutPresets.AddPreset(Preset);
					}
					else
					{
						OutMessages.Add(FString::Printf(TEXT("Duplicated preset on row %d of file %s (%s)"), RowNumber, *CurrentFile,*Preset.GUID.ToString()));
					}
				}
	
				Preset = FBIMPresetInstance();

				Preset.DEBUG_SourceFile = CurrentFile;
				Preset.DEBUG_SourceRow = RowNumber;

				if (KeyGuidMap.Find(presetID)!=nullptr)
				{
					OutMessages.Add(FString::Printf(TEXT("Duplicated BIM Key on row %d of file %s (%s)"), RowNumber, *CurrentFile, *Preset.GUID.ToString()));
				}

				Preset.PresetID = presetID;
				Preset.NodeType = NodeType.TypeName;
				Preset.NodeScope = NodeType.Scope;
				Preset.TypeDefinition = NodeType;

				// All presets have marks and comments
				FBIMPropertyKey propertyKey(EBIMValueScope::Preset, BIMPropertyNames::Mark);
				Preset.Properties.SetProperty<FString>(propertyKey.Scope, propertyKey.Name, FString());

				propertyKey = FBIMPropertyKey(EBIMValueScope::Preset, BIMPropertyNames::Comments);
				Preset.Properties.SetProperty<FString>(propertyKey.Scope, propertyKey.Name, FString());
			}
			else
			{
				bOnFirstRow = false;
			}
		}
	}

	//Handle GUID second so we can establish the guid<->key mapping
	for (auto& presetMatrix : PresetMatrices)
	{
		if (presetMatrix.Name == ECSVMatrixNames::GUID)
		{
			FString guidStr(Row[presetMatrix.First]);
			if (!guidStr.IsEmpty())
			{
				FGuid::ParseExact(guidStr, EGuidFormats::DigitsWithHyphens, Preset.GUID);
				if (GuidKeyMap.Contains(Preset.GUID))
				{
					FBIMKey* key = GuidKeyMap.Find(Preset.GUID);
					OutMessages.Add(FString::Printf(TEXT("Duplicated GUID on row %d of file %s (%s)"), RowNumber, *CurrentFile, *Preset.GUID.ToString()));
				}
			}
		}
	}

	if (ensureAlways(!Preset.PresetID.IsNone() && Preset.GUID.IsValid()))
	{
		KeyGuidMap.Add(Preset.PresetID, Preset.GUID);
		GuidKeyMap.Add(Preset.GUID, Preset.PresetID);
	}

	for (auto& presetMatrix : PresetMatrices)
	{
		switch (presetMatrix.Name)
		{
			case ECSVMatrixNames::ConstructionCost:
			{
				//Only get data from the first row (with preset ID present)
				if (bOnFirstRow)
				{
					FBIMConstructionCost constructionCost;
					GetOrCreateCustomData(constructionCost);
					constructionCost.MaterialCostRate = FCString::Atof(*NormalizeCell(Row[presetMatrix.First]));
					constructionCost.LaborCostRate = FCString::Atof(*NormalizeCell(Row[presetMatrix.First + 1]));
					Preset.PresetForm.AddConstructionCostElements();
					Preset.SetCustomData(constructionCost);
				}
			}
			break;
			case ECSVMatrixNames::PatternSegments:
			{
				FBIMPattern bimPattern;
				GetOrCreateCustomData(bimPattern);
				FString direction = NormalizeCell(Row[presetMatrix.First]);
				FString magnitude = NormalizeCell(Row[presetMatrix.First+1]);
				FString format = NormalizeCell(Row[presetMatrix.First + 2]);

				if (direction.Len() == 0 || magnitude.Len() == 0 || format.Len() == 0)
				{
					continue;
				}				

				FBIMPatternSequence* seq = nullptr;
				if (direction == TEXT("X"))
				{
					seq = &bimPattern.SequenceX;
				}
				else if (direction == TEXT("Y"))
				{
					seq = &bimPattern.SequenceY;
				}
				else if (direction == TEXT("Z"))
				{
					seq = &bimPattern.SequenceZ;
				}

				if (!ensureAlways(seq != nullptr))
				{
					OutMessages.Add(FString::Printf(TEXT("Unidentified direction %s in pattern %s"), *direction, *Preset.GUID.ToString()));
					continue;
				}

				FBIMPatternSegment& seg = seq->Segments.AddDefaulted_GetRef();
				if (!ensureAlways(FindEnumValueByString(format, seg.SegmentType)))
				{
					OutMessages.Add(FString::Printf(TEXT("Unidentified format %s in pattern %s"), *format, *Preset.GUID.ToString()));
				}
				seg.Value = FCString::Atof(*magnitude);

				Preset.SetCustomData(bimPattern);
			}
			break;

			case ECSVMatrixNames::PatternSpans:
			{
				FBIMPattern bimPattern;
				GetOrCreateCustomData(bimPattern);

				int32 matrixIndex = 0;

				FString id = NormalizeCell(Row[presetMatrix.First+ matrixIndex++]);
				FString element = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);

				TArray<FString> member;
				NormalizeCell(Row[presetMatrix.First + matrixIndex++]).ParseIntoArray(member, TEXT(","));

				TArray<FString> cutter;
				NormalizeCell(Row[presetMatrix.First + matrixIndex++]).ParseIntoArray(cutter,TEXT("="));

				FString ncp = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString origin = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString basisX = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString basisZ = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);

				FString offsetX = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString offsetY = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString offsetZ = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);

				FString rotationX = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString rotationY = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString rotationZ = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);

				FString flipX = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString flipY = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);
				FString flipZ = NormalizeCell(Row[presetMatrix.First + matrixIndex++]);

				if (!id.IsEmpty())
				{
					EBIMSpanElement elementType = EBIMSpanElement::None;
					if (ensureAlways(FindEnumValueByString< EBIMSpanElement>(element, elementType)))
					{
						auto& span = bimPattern.Spans.AddDefaulted_GetRef();
						span.ID = id;
						span.ElementType = elementType;
					}
					else
					{
						OutMessages.Add(FString::Printf(TEXT("Unidentified element format %s in pattern %s"), *element, *Preset.GUID.ToString()));
					}											
				}

				bimPattern.Spans.Last().Offset = FVector(FCString::Atof(*offsetX), FCString::Atof(*offsetY), FCString::Atof(*offsetZ));
				bimPattern.Spans.Last().Rotation = FRotator(FCString::Atof(*offsetX), FCString::Atof(*offsetY), FCString::Atof(*offsetZ));
				bimPattern.Spans.Last().Flip = FVector(flipX.IsEmpty() ? 1.0f : -1.0f, flipY.IsEmpty() ? 1.0f : -1.0f, flipZ.IsEmpty() ? 1.0f : -1.0f);

				if (!ncp.IsEmpty())
				{
					FGuid* hostedGuid = KeyGuidMap.Find(FBIMKey(ncp));
					if (ensureAlways(hostedGuid != nullptr))
					{
						bimPattern.Spans.Last().HostedPreset = *hostedGuid;
					}
					else
					{
						OutMessages.Add(FString::Printf(TEXT("Unidentified bim key %s in %s"), *ncp, *Preset.GUID.ToString()));
					}
				}

				TArray<FString> originCoords;
				origin.ParseIntoArray(originCoords,TEXT(","));
				if (originCoords.Num() > 0)
				{
					bimPattern.Spans.Last().Origin.X = FCString::Atof(*originCoords[0]);
					if (originCoords.Num() > 1)
					{
						bimPattern.Spans.Last().Origin.Y = FCString::Atof(*originCoords[1]);
						if (originCoords.Num() > 2)
						{
							bimPattern.Spans.Last().Origin.Z = FCString::Atof(*originCoords[2]);
						}
					}
				}

				if (member.Num() > 0)
				{
					auto& newElement = bimPattern.Spans.Last().Elements.AddDefaulted_GetRef();
					newElement.FromCSVCells(member);
				}

				auto assignBasis = [this,&OutMessages](const FString& BasisStr, FBIMSpanBasis& OutBasis)
				{
					if (BasisStr.IsEmpty())
					{
						return;
					}
					if (BasisStr == TEXT("ParentFaceNormal"))
					{
						OutBasis.Element.ElementType = EBIMSpanElement::ParentFaceNormal;
					}
					else
					{
						TArray<FString> elementCoord;
						BasisStr.ParseIntoArray(elementCoord, TEXT("="));
						if (ensureAlways(elementCoord.Num() == 2))
						{
							EBIMSpanElement elementType;
							TArray<FString> coord;
							elementCoord[1].ParseIntoArray(coord, TEXT(","));
							if (ensureAlways(FindEnumValueByString(elementCoord[0], elementType)))
							{
								OutBasis.Element.ElementType = elementType;
								if (coord.Num() > 0)
								{
									OutBasis.Element.Coordinate.X = FCString::Atoi(*coord[0]);
									if (coord.Num() > 1)
									{
										OutBasis.Element.Coordinate.Y = FCString::Atoi(*coord[1]);
										if (coord.Num() > 2)
										{
											OutBasis.Element.Coordinate.Z = FCString::Atoi(*coord[2]);
										}
									}
								}
							}
							else
							{
								OutMessages.Add(FString::Printf(TEXT("Bad basis format %s in pattern %s"), *BasisStr, *Preset.GUID.ToString()));
							}
						}
						else
						{
							OutMessages.Add(FString::Printf(TEXT("Bad basis format %s in pattern %s"), *BasisStr, *Preset.GUID.ToString()));
						}
					}
				};

				assignBasis(basisX, bimPattern.Spans.Last().BasisX);
				assignBasis(basisZ, bimPattern.Spans.Last().BasisZ);

				if (cutter.Num() == 2)
				{
					if (bimPattern.Spans.Num() == 0)
					{
						OutMessages.Add(FString::Printf(TEXT("Cutter out of order at row %d of file %f"), RowNumber, *CurrentFile));
					}
					else
					{
						EBIMSpanElement cutterType;
						if (ensureAlways(FindEnumValueByString<EBIMSpanElement>(cutter[0], cutterType)))
						{
							auto& cutterData = bimPattern.Spans.Last().Cutters.AddDefaulted_GetRef();
							cutterData.ElementType = cutterType;
							TArray<FString> coords;
							cutter[1].ParseIntoArray(coords, TEXT(","));
							cutterData.Coordinate.FromCSVCells(coords);
						}
						else
						{
							OutMessages.Add(FString::Printf(TEXT("Unidentified cutter format %s in pattern %s"), *cutter[1], *Preset.GUID.ToString()));
						}
					}
				}
				Preset.SetCustomData(bimPattern);
			}
			break;

			case ECSVMatrixNames::EdgeDetail:
			{
				Preset.SetCustomData(FEdgeDetailData(FEdgeDetailData::CurrentVersion));
			}
			break;

			case ECSVMatrixNames::MiterPriority:
			{
				FString group = NormalizeCell(Row[presetMatrix.First]);
				FBIMPresetLayerPriority miterPriority;

				if (FindEnumValueByString(group, miterPriority.PriorityGroup) && LexTryParseString(miterPriority.PriorityValue, Row[presetMatrix.First + 1]))
				{
					miterPriority.SetFormElements(Preset.PresetForm);
					Preset.SetCustomData(miterPriority);
				}				
			}
			break;

			// TODO: expand this matrix when new pattern language is developed...for now, just tuck the pattern 1.0 guid
			case ECSVMatrixNames::Pattern:
			{
				FBIMKey profileKey(NormalizeCell(Row[presetMatrix.First]));
				if (!profileKey.IsNone())
				{
					const FGuid* patternGuid = KeyGuidMap.Find(profileKey);
					if (ensureAlways(patternGuid != nullptr))
					{
						Preset.Properties.SetProperty(EBIMValueScope::Pattern, BIMPropertyNames::AssetID, patternGuid->ToString());
						Preset.PresetForm.AddPropertyElement(LOCTEXT("BIMPattern","Pattern"), FBIMPropertyKey(EBIMValueScope::Pattern, BIMPropertyNames::AssetID).QN(), EBIMPresetEditorField::AssetProperty);
					}
				}
			}
			break;

			case ECSVMatrixNames::Profile:
			{
				FBIMKey profileKey(NormalizeCell(Row[presetMatrix.First]));
				if (!profileKey.IsNone())
				{
					FGuid* guid = KeyGuidMap.Find(profileKey);
					if (ensureAlways(guid != nullptr))
					{
						Preset.Properties.SetProperty(EBIMValueScope::Profile, BIMPropertyNames::AssetID, guid->ToString());
						Preset.PresetForm.AddPropertyElement(LOCTEXT("BIMProfile","Profile"), FBIMPropertyKey(EBIMValueScope::Profile, BIMPropertyNames::AssetID).QN(), EBIMPresetEditorField::AssetProperty);
					}
				}
			}
			break;

			case ECSVMatrixNames::Mesh:
			{
				FBIMKey meshKey(NormalizeCell(Row[presetMatrix.First]));
				if (!meshKey.IsNone())
				{
					FGuid* guid = KeyGuidMap.Find(meshKey);
					if (ensureAlways(guid != nullptr))
					{
						Preset.Properties.SetProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetID, guid->ToString());
						Preset.PresetForm.AddPropertyElement(FText::FromString(TEXT("Mesh")), FBIMPropertyKey(EBIMValueScope::Mesh, BIMPropertyNames::AssetID).QN(), EBIMPresetEditorField::AssetProperty);

						/*
						* Meshes provide default values for named dimensions required by parts
						* No need to add these properties to the form template 
						* The node editor builds forms only for named dimensions that are visible
						*/
						const FBIMPresetInstance* meshPreset = OutPresets.PresetFromGUID(*guid);
						if (ensureAlways(meshPreset != nullptr))
						{
							meshPreset->Properties.ForEachProperty([this](const FBIMPropertyKey& Key, float Value) {
								if (Key.Scope == EBIMValueScope::Dimension && !Preset.Properties.HasProperty<float>(EBIMValueScope::Dimension,Key.Name))
								{
									Preset.Properties.SetProperty(Key.Scope, Key.Name, Value);
								}
							});
						}
					}
				}
			}
			break;

			case ECSVMatrixNames::Material :
			{
				FBIMPresetMaterialBinding materialBinding;
				for (int32 i = 0; i < presetMatrix.Columns.Num(); ++i)
				{
					const FString& fieldString = presetMatrix.Columns[i];
					EMaterialChannelFields targetEnum = GetEnumValueByString<EMaterialChannelFields>(fieldString);
					switch (targetEnum)
					{
						case EMaterialChannelFields::AppliesToChannel:
						{
							materialBinding.Channel = Row[presetMatrix.First + i];
						}
						break;

						case EMaterialChannelFields::InnerMaterial:
						{
							FBIMKey matKey(NormalizeCell(Row[presetMatrix.First + i]));
							if (!matKey.IsNone())
							{
								const FGuid* guid = KeyGuidMap.Find(matKey);
								if (ensureAlways(guid != nullptr))
								{
									materialBinding.InnerMaterialGUID = *guid;
								}
							}
						}
						break;

						case EMaterialChannelFields::SurfaceMaterial:
						{
							FBIMKey matKey(NormalizeCell(Row[presetMatrix.First + i]));
							if (!matKey.IsNone())
							{
								const FGuid* guid = KeyGuidMap.Find(matKey);
								if (ensureAlways(guid != nullptr))
								{
									materialBinding.SurfaceMaterialGUID = *guid;
								}
							}
						}
						break;

						case EMaterialChannelFields::ColorTint:
						{
							FString hexValue(Row[presetMatrix.First + i]);
							if (!hexValue.IsEmpty())
							{
								Preset.Properties.SetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, hexValue);
								materialBinding.ColorHexValue = hexValue;
							}
						}
						break;

						case EMaterialChannelFields::ColorTintVariation:
						{
							LexTryParseString(materialBinding.ColorTintVariationPercent, Row[presetMatrix.First + i]);
						}
						break;

						default:
							ensureAlways(false);
					};
				}

				if (!materialBinding.Channel.IsNone())
				{
					FBIMPresetMaterialBindingSet bindingSet;
					Preset.TryGetCustomData(bindingSet); // okay that this fails on first binding
					bindingSet.MaterialBindings.Add(materialBinding);
					Preset.SetCustomData(bindingSet);

					FGuid material = materialBinding.SurfaceMaterialGUID.IsValid() ? materialBinding.SurfaceMaterialGUID : materialBinding.InnerMaterialGUID;

					if (ensureAlways(material.IsValid()))
					{
						Preset.Properties.SetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, material.ToString());
					}

					Preset.Properties.SetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, materialBinding.ColorHexValue.IsEmpty() ? FColor::White.ToHex() : materialBinding.ColorHexValue);
					bindingSet.SetFormElements(Preset.PresetForm);
				}
			}
			break;

			case ECSVMatrixNames::Dimensions : 
			{
				FName dimName(Row[presetMatrix.First]);
				if (!dimName.IsNone())
				{
					FModumateFormattedDimension measurement = UModumateDimensionStatics::StringToFormattedDimension(Row[presetMatrix.First + 1]);
					Preset.Properties.SetProperty(EBIMValueScope::Dimension, dimName, static_cast<float>(measurement.Centimeters));
					Preset.PresetForm.AddPropertyElement(FText::FromString(Row[presetMatrix.First]), FBIMPropertyKey(EBIMValueScope::Dimension, dimName).QN(), EBIMPresetEditorField::DimensionProperty);
				}
			}
			break;

			case ECSVMatrixNames::StartsInProject:
			{
				if (ensureAlways(Preset.GUID.IsValid()))
				{
					FString cellStr = Row[presetMatrix.First];
					if (!cellStr.IsEmpty())
					{
						OutStarters.Add(Preset.GUID);
					}
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
								Preset.SetScopedProperty(propSpec.Scope, propSpec.Name, static_cast<float>(UModumateDimensionStatics::StringToFormattedDimension(cell).Centimeters));
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
					Preset.MyTagPath.FromString(NormalizeCell(ncp));
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
					if (cell.IsEmpty())
					{
						continue;
					}

					if (category.Equals(TEXT("SlotConfig")))
					{
						FBIMKey key(NormalizeCell(*cell));
						FGuid* guid = KeyGuidMap.Find(key);
						if (ensureAlways(guid != nullptr))
						{
							Preset.SlotConfigPresetGUID = *guid;
							Preset.SlotConfigPresetID = key;
						}
					}
					else if (category.Equals(TEXT("PartPreset")))
					{
						if (Preset.PartSlots.Num() == 0 || Preset.PartSlots.Last().PartPresetGUID.IsValid())
						{
							Preset.PartSlots.AddDefaulted();
						}
						FBIMKey bimKey(NormalizeCell(cell));
						FGuid* guid = KeyGuidMap.Find(bimKey);
						if (ensureAlways(guid != nullptr))
						{
							Preset.PartSlots.Last().PartPresetGUID = *guid;
							Preset.PartSlots.Last().PartPreset = bimKey;
						}
					}
					else if (category.Equals(TEXT("SlotName")))
					{
						if (Preset.PartSlots.Num() == 0 || Preset.PartSlots.Last().SlotPresetGUID.IsValid())
						{
							Preset.PartSlots.AddDefaulted();
						}
						FBIMKey key(NormalizeCell(cell));
						FGuid* guid = KeyGuidMap.Find(key);
						if (ensureAlways(guid != nullptr))
						{
							Preset.PartSlots.Last().SlotPresetGUID = *guid;
							Preset.PartSlots.Last().SlotPreset = key;
						}
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

							FGuid* guid = KeyGuidMap.Find(newCAP.PresetID);
							if (ensureAlways(guid != nullptr))
							{
								newCAP.PresetGUID = *guid;
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
		}
	}

	return EBIMResult::Success;
}

EBIMResult FBIMCSVReader::ProcessPropertyDeclarationRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages)
{
	//Row Format:
	//[PROPERTY][][Property Type][][Property Name][][Property Value]
	FName propertyTypeName(Row[2]);
	FString displayName(Row[4]);
	EBIMValueType propertyTypeEnum;
	FBIMPropertyKey propertyKey(Row[6]);

	if (!FindEnumValueByName(propertyTypeName, propertyTypeEnum))
	{
		return EBIMResult::Error;
	}

	PropertyTypeMap.Add(propertyKey.QN(), propertyTypeEnum);

	if (propertyKey.Scope == EBIMValueScope::None)
	{
		return EBIMResult::Error;
	}
	else
	{
		NodeType.Properties.SetProperty(propertyKey.Scope, propertyKey.Name, FString());

		//Properties are only visible in the editor if they nave display names
		if (!displayName.IsEmpty())
		{
			NodeType.FormTemplate.AddPropertyElement(FText::FromString(displayName), propertyKey.QN(), EBIMPresetEditorField::TextProperty);
		}
	}
	return EBIMResult::Success;
}

#undef LOCTEXT_NAMESPACE