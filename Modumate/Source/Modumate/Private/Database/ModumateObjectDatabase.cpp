// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectDatabase.h"

#include "Components/StaticMeshComponent.h"
#include "EditModelGameMode_CPP.h"
#include "Engine/StaticMeshActor.h"
#include "ExpressionEvaluator.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ModumateDecisionTreeImpl.h"
#include "ModumateDataTables.h"
#include "ModumateFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "ModumateCraftingTreeBuilder.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"


namespace Modumate
{
	ModumateObjectDatabase::ModumateObjectDatabase() {}

	void ModumateObjectDatabase::ReadCraftingSubcategoryData(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}
		data->ForeachRow<FCraftingSubcategoryDataTable>(TEXT("FCraftingSubcategoryData"),
			[this](
				const FName &Key,
				const FCraftingSubcategoryDataTable &data)
		{
			FCraftingSubcategoryData newData;
			newData.Key = Key;
			newData.SheetName = data.SheetName;
			newData.DisplayName = data.DisplayName;
			newData.IDCodeLine1 = data.IDCodeLine1;
			newData.LayerFormat = data.Format;
			newData.LayerFunction = data.LayerFunction;

			if (data.CraftingIconAssetFilePath != nullptr)
			{
				newData.Icon = Cast<UTexture2D>(data.CraftingIconAssetFilePath.TryLoad());
				if (ensureMsgf(newData.Icon.IsValid(),
					TEXT("Failed to load icon %s"), *data.CraftingIconAssetFilePath.ToString()))
				{
					// TODO: replace with proper asset reference and lazy-loading solution
					newData.Icon->AddToRoot();
				}
			}

			CraftingSubcategoryData.AddData(newData);
		});
	}

	void ModumateObjectDatabase::ReadCraftingMaterialAndColorOptionSet(UDataTable *data)
	{
		TArray<FString> supportedSets;

		ReadOptionSet<
			FMaterialsAndColorsOptionSetDataTable,
			FCraftingOptionBase,
			FCraftingOptionSet>(data,
				[&supportedSets,this](const FMaterialsAndColorsOptionSetDataTable &row, FCraftingOptionBase &matcol)
			{
				if (row.SupportedOptionSets.Num() > 0)
				{
					supportedSets = row.SupportedOptionSets;
				}

				if (row.Material.IsEmpty())
				{
					return false;
				}

				const FArchitecturalMaterial *mat = AMaterials.GetData(*row.Material);

				if (ensureAlwaysMsgf(mat != nullptr, TEXT("Could not find material %s in material and color %s"), *row.Material, *row.DisplayName.ToString()))
				{
					matcol.EngineMaterial = mat->EngineMaterial;
						
					const FCustomColor *col = NamedColors.GetData(*row.BaseColor);
					if (ensureAlwaysMsgf(col != nullptr, TEXT("Could not find color %s in material and color %s"), *row.BaseColor, *row.DisplayName.ToString()))
					{
						matcol.DisplayName = FText::FromString(mat->DisplayName.ToString() + TEXT(", ")+col->DisplayName.ToString());
						matcol.CustomColor = *col;
					}
					else
					{
						matcol.DisplayName = mat->DisplayName;
					}
				}
				else
				{
					matcol.DisplayName = FText::FromString(row.Material + row.BaseColor);
				}

				matcol.CraftingParameters.SetValue(Modumate::CraftingParameters::MaterialColorMaterial, row.Material);
				matcol.CraftingParameters.SetValue(Modumate::CraftingParameters::MaterialColorColor, row.BaseColor);
				return true;
			},
				[this, &supportedSets](const FCraftingOptionSet &os)
			{
				for (auto &sos : supportedSets)
				{
					if (sos.Contains(TEXT("Finish")))
					{
						FCraftingOptionSet finishSet = os;
						finishSet.Key = *sos;
						FinishMaterialColors.AddData(finishSet);
					}
					else
					if (sos.Contains(TEXT("Module")))
					{
						ModuleMaterialColors.AddData(os);
					}
					else if (sos.Contains(TEXT("Gap")))
					{
						GapMaterialColors.AddData(os);
					}
					else
					{
						LayerMaterialColors.AddData(os);
					}
				}
			}
		);
	}

	void ModumateObjectDatabase::ReadCraftingPortalPartOptionSet(UDataTable *data)
	{
		ReadOptionSet<
			FPortalPartOptionsSetDataTable, 
			FCraftingPortalPartOption,
			FCraftingPortalPartOptionSet>
		(
			data, 
			[this](const FPortalPartOptionsSetDataTable &row, FCraftingPortalPartOption &po) 
			{
				po.DisplayName = row.DisplayName;
				po.ConfigurationWhitelist.Append(row.ConfigurationWhitelist);

				static const FName displayNameProp(TEXT("DisplayName"));
				static const FName subcatNameProp(TEXT("SupportedSubcategories"));
				static const FName configsNameProp(TEXT("ConfigurationWhitelist"));
				static const EPropertyFlags skipFlags = CPF_Deprecated | CPF_Transient;
				const UStruct* rowStructDef = FPortalPartOptionsSetDataTable::StaticStruct();
				for (TFieldIterator<FProperty> propIter(rowStructDef); propIter; ++propIter)
				{
					FProperty* rowProp = *propIter;
					if ((rowProp == nullptr) || rowProp->HasAnyPropertyFlags(skipFlags))
					{
						continue;
					}

					FName propName = rowProp->GetFName();
					if ((propName == displayNameProp) || (propName == subcatNameProp) || (propName == configsNameProp))
					{
						continue;
					}

					const FName *slotPartName = rowProp->ContainerPtrToValuePtr<FName>(&row);

					EPortalSlotType propSlotType;
					bool bValidEnum = TryEnumValueByName(EPortalSlotType, propName, propSlotType);
					if (ensureAlways(bValidEnum && slotPartName) && !slotPartName->IsNone())
					{
						const FPortalPart *portalPart = PortalParts.GetData(*slotPartName);
						if (ensureAlways(portalPart))
						{
							po.PartsBySlotType.Add(propSlotType, *slotPartName);
						}
					}
				}
				return true;
			},
			[this](const FCraftingPortalPartOptionSet &pos)
			{
				PortalPartOptionSets.AddData(pos);
			}
		);
	}

	void ModumateObjectDatabase::ReadCraftingPatternOptionSet(UDataTable *data)
	{
		ReadOptionSet<FPatternOptionSetDataTable,
			FCraftingOptionBase,
			FCraftingOptionSet>(data,
				[this](const FPatternOptionSetDataTable &row, FCraftingOptionBase &option)
				{
					option.DisplayName = row.DisplayName;

					option.CraftingParameters.SetValue(Modumate::CraftingParameters::PatternModuleCount, FCString::Atoi(*row.ModuleCount));
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::PatternExtents, row.Extents);
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::PatternThickness, row.PatternThickness);
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::PatternGap, row.Gap);
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::PatternName, row.DisplayName.ToString());

					TArray<FString> moduleDimensions;

					if (row.CraftingIconAssetFilePath != nullptr)
					{
						option.Icon = Cast<UTexture2D>(row.CraftingIconAssetFilePath.TryLoad());

						if (ensureMsgf(option.Icon.IsValid(),
							TEXT("Failed to load icon %s"), *row.CraftingIconAssetFilePath.ToString()))
						{
							// TODO: replace with proper asset reference and lazy-loading solution
							option.Icon->AddToRoot();

							FStaticIconTexture staticIcon;
							staticIcon.Texture = option.Icon;
							// Uses name of texture asset as key
							staticIcon.Key = option.Icon->GetFName();
							staticIcon.DisplayName = option.DisplayName;
							StaticIconTextures.AddData(staticIcon);
						}

						FString rowPropName;
						const UStruct* rowStructDef = FPatternOptionSetDataTable::StaticStruct();
						for (TFieldIterator<FProperty> propIter(rowStructDef); propIter; ++propIter)
						{
							FProperty* rowProp = *propIter;

							static const FString modulePropertyPrefix(TEXT("Module"));
							static const FString modulePropertySuffix(TEXT("Dimensions"));
							rowProp->GetName(rowPropName);
							if (rowPropName.RemoveFromStart(modulePropertyPrefix, ESearchCase::CaseSensitive) &&
								rowPropName.RemoveFromEnd(modulePropertySuffix, ESearchCase::CaseSensitive))
							{
								const FString *moduleDimensionString = rowProp->ContainerPtrToValuePtr<FString>(&row);
								int32 modulePropIdx = FCString::Atoi(*rowPropName) - 1;

								if (moduleDimensionString && !moduleDimensionString->IsEmpty() &&
									ensureAlways(modulePropIdx == moduleDimensions.Num()))
								{
									moduleDimensions.Add(*moduleDimensionString);
								}
							}
						}
					}

					option.CraftingParameters.SetValue(Modumate::CraftingParameters::PatternModuleDimensions, moduleDimensions);

					// Now, verify that the pattern expressions can evaluate correctly
					FLayerPattern pattern;
					pattern.InitFromCraftingParameters(option.CraftingParameters);

					// Test pattern evaluation with fake numbers
					TMap<FString, float> patternExprVars;
					patternExprVars.Add(FString(TEXT("G")), PI);
					for (int32 moduleIdx = 0; moduleIdx < pattern.ModuleCount; ++moduleIdx)
					{
						FVector moduleDims(UE_GOLDEN_RATIO);
						UModumateFunctionLibrary::PopulatePatternModuleVariables(patternExprVars, FVector::OneVector, moduleIdx);
					}

					static const FString commaStr(TEXT(","));
					FString extentsExpressions = pattern.ParameterizedExtents;
					extentsExpressions.RemoveFromStart(TEXT("("));
					extentsExpressions.RemoveFromEnd(TEXT(")"));

					FString patternWidthExpr, patternHeightExpr;
					if (extentsExpressions.Split(commaStr, &patternWidthExpr, &patternHeightExpr))
					{
						FVector patternExtentsValue(ForceInitToZero);
						ensureAlwaysMsgf(
							Expression::Evaluate(patternExprVars, patternWidthExpr, patternExtentsValue.X) &&
							Expression::Evaluate(patternExprVars, patternHeightExpr, patternExtentsValue.Y),
							TEXT("Pattern %s failed to evaluate module extents expression: %s"),
							*option.DisplayName.ToString(), *pattern.ParameterizedExtents);
					}

					int32 numModuleTiles = pattern.ParameterizedModuleDimensions.Num();
					for (int32 moduleInstIdx = 0; moduleInstIdx < numModuleTiles; ++moduleInstIdx)
					{
						auto &moduleInstData = pattern.ParameterizedModuleDimensions[moduleInstIdx];
						float moduleX, moduleY, moduleW, moduleH;

						ensureAlwaysMsgf(
							Expression::Evaluate(patternExprVars, moduleInstData.ModuleXExpr, moduleX) &&
							Expression::Evaluate(patternExprVars, moduleInstData.ModuleYExpr, moduleY) &&
							Expression::Evaluate(patternExprVars, moduleInstData.ModuleWidthExpr, moduleW) &&
							Expression::Evaluate(patternExprVars, moduleInstData.ModuleHeightExpr, moduleH),
							TEXT("Pattern %s failed to evaluate module #%d expression(s)!"),
							*option.DisplayName.ToString(), moduleInstIdx);
					}

					return true;
				},
				[this](const FCraftingOptionSet &os)
				{
					const FCraftingOptionSet *optionSet = PatternOptionSets.GetData(os.Key);
					if (optionSet != nullptr)
					{
						FCraftingOptionSet newSet = *optionSet;
						newSet.Options.Append(os.Options);
						PatternOptionSets.AddData(newSet);
					}
					else
					{
						PatternOptionSets.AddData(os);
					}
				});
	}

	void ModumateObjectDatabase::ReadCraftingDimensionalOptionSet(UDataTable *data)
	{
		static const FName optionTypeModule(TEXT("Module"));
		static const FName optionTypeGap(TEXT("Gap"));
		static const FName optionTypeToeKick(TEXT("Toe Kick"));
		FName dimensionalOptionType;

		ReadOptionSet<FDimensionalOptionSetDataTable,
			FCraftingOptionBase,
			FCraftingOptionSet>(data,
				[&dimensionalOptionType,this](const FDimensionalOptionSetDataTable &row, FCraftingOptionBase &option)
				{
					if (!row.DimensionalOptionType.IsNone())
					{
						dimensionalOptionType = row.DimensionalOptionType;
					}

					auto getUnitValue = [](float v, const FString &u)
					{
						if (u == TEXT("cm"))
						{
							return Modumate::Units::FUnitValue::WorldCentimeters(v);
						}
						return Modumate::Units::FUnitValue::WorldInches(v);
					};

					option.DisplayName = row.DisplayName;

					option.CraftingParameters.SetValue(Modumate::CraftingParameters::DimensionLength, getUnitValue(row.Length, row.LUnits).AsWorldCentimeters());
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::DimensionWidth, getUnitValue(row.Width, row.WUnits).AsWorldCentimeters());
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::DimensionHeight, getUnitValue(row.Height, row.HUnits).AsWorldCentimeters());
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::DimensionDepth, getUnitValue(row.Depth, row.DUnits).AsWorldCentimeters());
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::DimensionThickness, getUnitValue(row.Thickness, row.TUnits).AsWorldCentimeters());
					option.CraftingParameters.SetValue(Modumate::CraftingParameters::DimensionBevelWidth, getUnitValue(row.BevelWidth, row.BevelUnit).AsWorldCentimeters());

					if (row.CraftingIconAssetFilePath != nullptr)
					{
						option.Icon = Cast<UTexture2D>(row.CraftingIconAssetFilePath.TryLoad());
						if (ensureMsgf(option.Icon.IsValid(),
							TEXT("Failed to load icon %s"), *row.CraftingIconAssetFilePath.ToString()))
						{
							// TODO: replace with proper asset reference and lazy-loading solution
							option.Icon->AddToRoot();
						}
					}
					return true;
				},
				[this,&dimensionalOptionType](const FCraftingOptionSet &os)
				{
					if (dimensionalOptionType == optionTypeModule)
					{
						LayerModuleOptionSets.AddData(os);
					}
					else if (dimensionalOptionType == optionTypeGap)
					{
						LayerGapOptionSets.AddData(os);
					}
					else if (dimensionalOptionType == optionTypeToeKick)
					{
						ToeKickOptionSets.AddData(os);
					}
				});
	}

	void ModumateObjectDatabase::ReadCraftingLayerThicknessOptionSet(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		ReadOptionSet<
			FLayerThicknessOptionSetDataTable,
			FCraftingOptionBase,
			FCraftingOptionSet>(data,
				[this](const FLayerThicknessOptionSetDataTable &row, FCraftingOptionBase &option)
		{
			option.DisplayName = row.DisplayName;
			option.CraftingParameters.SetValue(Modumate::CraftingParameters::ThicknessValue, row.Thickness);
			option.CraftingParameters.SetValue(Modumate::CraftingParameters::ThicknessUnits, row.ThicknessUnits);
			return true;
		},
				[this](const FCraftingOptionSet &optionSet)
		{
			LayerThicknessOptionSets.AddData(optionSet);
		});
	}

	void ModumateObjectDatabase::ReadCraftingProfileOptionSet(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		ReadOptionSet<
			FProfileOptionSetTableRow,
			FCraftingOptionBase,
			FCraftingOptionSet>(data,
				[this](const FProfileOptionSetTableRow &row, FCraftingOptionBase &option)
		{
			option.DisplayName = row.DisplayName;

			// TODO: also add lazy loading support here, as if it were a regular UStaticMesh
			option.ProfileMesh.Key = option.Key;
			option.ProfileMesh.AssetPath = row.AssetPath;
			option.ProfileMesh.Asset = Cast<USimpleMeshData>(option.ProfileMesh.AssetPath.TryLoad());
			if (ensureAlwaysMsgf(option.ProfileMesh.Asset.IsValid(),
				TEXT("Failed to load trim profile polygon asset: \"%s\""), *option.ProfileMesh.AssetPath.GetAssetPathString()))
			{
				option.ProfileMesh.Asset->AddToRoot();
			}

			if (row.CraftingIconAssetFilePath.IsAsset())
			{
				option.Icon = Cast<UTexture2D>(row.CraftingIconAssetFilePath.TryLoad());
			}
			SimpleMeshes.AddData(option.ProfileMesh);

			return true;
		},
		[this](const FCraftingOptionSet &optionSet)
		{
			ProfileOptionSets.AddData(optionSet);
		});
	}

	void ModumateObjectDatabase::ReadRoomConfigurations(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		data->ForeachRow<FRoomConfigurationTableRow>(TEXT("FRoomConfigurationTableRow"),
			[this]
		(const FName &Key, const FRoomConfigurationTableRow &Row)
		{
			if (!Key.IsNone())
			{
				FRoomConfiguration newRow;
				newRow.UseGroupCode = Row.UseGroupCode;
				newRow.UseGroupType = Row.UseGroupType;
				newRow.DisplayName = Row.DisplayName;
				newRow.OccupantLoadFactor = Row.OccupantLoadFactor;
				newRow.AreaType = Row.AreaType;
				newRow.LoadFactorSpecialCalc = Row.LoadFactorSpecialCalc;
				newRow.HexValue = Row.HexValue;

				newRow.DatabaseKey = Key;

				RoomConfigurations.AddData(newRow);
			}
		});
	}

	/*
	Portal (Door/Window) part data
	*/
	void ModumateObjectDatabase::ReadPortalPartData(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		data->ForeachRow<FPortalPartTableRow>(TEXT("FPortalPartRow"),
			[this]
			(const FName &Key, const FPortalPartTableRow &Row)
		{
			if (!Key.IsNone() && !Row.DisplayName.IsEmpty() && !Row.Mesh.IsEmpty())
			{
				const FArchitecturalMesh *mesh = AMeshes.GetData(*Row.Mesh);
				if (ensureAlwaysMsgf(mesh, TEXT("Mesh %s not found for PortalPart %s!"),
					*Row.Mesh, *Key.ToString()))
				{
					FPortalPart newPortalPart;

					newPortalPart.Key = Key;
					newPortalPart.DisplayName = Row.DisplayName;
					newPortalPart.SupportedSlotTypes.Append(Row.SupportedSlotTypes);
					newPortalPart.Mesh = *mesh;

					newPortalPart.NativeSizeX = Modumate::Units::FUnitValue::WorldInches(Row.NativeSizeX);
					newPortalPart.NativeSizeY = Modumate::Units::FUnitValue::WorldInches(Row.NativeSizeY);
					newPortalPart.NativeSizeZ = Modumate::Units::FUnitValue::WorldInches(Row.NativeSizeZ);

					auto nineSliceX1 = Modumate::Units::FUnitValue::WorldInches(Row.NineSliceX1);
					auto nineSliceX2 = Modumate::Units::FUnitValue::WorldInches(Row.NineSliceX2);
					auto nineSliceZ1 = Modumate::Units::FUnitValue::WorldInches(Row.NineSliceZ1);
					auto nineSliceZ2 = Modumate::Units::FUnitValue::WorldInches(Row.NineSliceZ2);

					if ((Row.NineSliceX1 != 0.0f) || (Row.NineSliceX2 != 0.0f) || (Row.NineSliceZ1 != 0.0f) || (Row.NineSliceZ2 != 0.0f))
					{
						newPortalPart.NineSliceInterior = FBox(
							FVector(nineSliceX1.AsWorldCentimeters(), 0.0f, nineSliceZ1.AsWorldCentimeters()),
							FVector(nineSliceX2.AsWorldCentimeters(), newPortalPart.NativeSizeY.AsWorldCentimeters(), nineSliceZ2.AsWorldCentimeters()));
					}

					for (const auto &kvp : Row.ConfigurationDimensions)
					{
						newPortalPart.ConfigurationDimensions.Add(kvp.Key,
							Modumate::Units::FUnitValue::WorldInches(kvp.Value));
					}

					newPortalPart.SupportedMaterialChannels.Append(Row.SupportedMaterialChannels);

					PortalParts.AddData(newPortalPart);
				}
			}
		});
	}

	/*
	Portal (Door/Window) configurations
	*/
	void ModumateObjectDatabase::ReadPortalConfigurationData(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		static const FString subCatPrefix(TEXT("Subcategories"));
		const static TArray<TPair<FString, EObjectType>> portalTypeNames = {
			TPairInitializer<const FString&, const EObjectType&>(TEXT("Door"), EObjectType::OTDoor),
			TPairInitializer<const FString&, const EObjectType&>(TEXT("Window"), EObjectType::OTWindow),
			TPairInitializer<const FString&, const EObjectType&>(TEXT("Cabinet"), EObjectType::OTCabinet)
		};

		EObjectType curObjectType = EObjectType::OTUnknown;
		EPortalFunction curPortalFunction = EPortalFunction::None;
		FPortalAssemblyConfigurationOption pendingConfig;
		FPortalAssemblyConfigurationOptionSet pendingSet;
		bool bReadingConfig = false;
		FString subcategoryString;

		data->ForeachRow<FPortalConfigurationTableRow>(TEXT("FPortalConfigurationTableRow"),
			[this, &pendingSet, &subcategoryString, &curObjectType, &curPortalFunction, &pendingConfig, &bReadingConfig]
			(const FName &Key,
			const FPortalConfigurationTableRow &Row)
		{
			if (!Row.SupportedSubcategory.IsEmpty())
			{
				if (pendingSet.Options.Num() != 0)
				{
					PortalConfigurationOptionSets.AddData(pendingSet);
					pendingSet.Options.Empty();
				}
				subcategoryString = Row.SupportedSubcategory;
				pendingSet.Key = *subcategoryString;
				if (ensureAlwaysMsgf(subcategoryString.RemoveFromStart(subCatPrefix, ESearchCase::CaseSensitive),
					TEXT("Subcategory must start with prefix: \"%s\""), *subCatPrefix))
				{
					curObjectType = EObjectType::OTUnknown;
					const FString *objectPrefix = nullptr;

					for (auto &kvp : portalTypeNames)
					{
						if (subcategoryString.StartsWith(kvp.Key, ESearchCase::CaseSensitive))
						{
							curObjectType = kvp.Value;
							subcategoryString.RemoveFromStart(kvp.Key);
							break;
						}
					}

					ensureAlways((curObjectType != EObjectType::OTUnknown) &&
						TryEnumValueByString(EPortalFunction, subcategoryString, curPortalFunction));
				}
			}

			if (!Row.ConfigurationDisplayName.IsEmpty() && ensureAlways(
				!bReadingConfig && (curPortalFunction != EPortalFunction::None)))
			{
				bReadingConfig = true;
				pendingConfig = FPortalAssemblyConfigurationOption();
				pendingConfig.PortalFunction = curPortalFunction;
				if (Row.CraftingIconAssetFilePath.IsAsset())
				{
					pendingConfig.Icon = Cast<UTexture2D>(Row.CraftingIconAssetFilePath.TryLoad());
				}

				pendingConfig.DisplayName = FText::FromString(Row.ConfigurationDisplayName.ToString());

				pendingConfig.Key = FName(*FString::Printf(TEXT("%s-%s-%s-%s"),
					*Key.ToString(),
					*(EnumValueString(EObjectType, curObjectType).RightChop(2)),
					*(EnumValueString(EPortalFunction, curPortalFunction)),
					*Row.ConfigurationDisplayName.ToString()
				));

				static const FString widthTypeString(TEXT("Width")), heightTypeString(TEXT("Height"));
				ParsePortalConfigDimensionSets(pendingConfig.Key, widthTypeString, Row.SupportedWidths, pendingConfig.SupportedWidths);
				ParsePortalConfigDimensionSets(pendingConfig.Key, heightTypeString, Row.SupportedHeights, pendingConfig.SupportedHeights);
			}

			if (bReadingConfig)
			{
				if (Row.SlotType.IsEmpty())
				{
					if (ensureAlwaysMsgf(pendingConfig.IsValid(),
						TEXT("Portal configuration %s is invalid/incomplete!"), *pendingConfig.Key.ToString()))
					{
						pendingSet.Options.Add(pendingConfig);
					}
					bReadingConfig = false;
				}
				else
				{
					FPortalAssemblyConfigurationSlot newPortalSlot;
					EPortalSlotType partSlotType = EPortalSlotType::None;

					// Parse a regular portal part
					if (TryEnumValueByString(EPortalSlotType, Row.SlotType, partSlotType))
					{
						FPortalAssemblyConfigurationSlot portalConfigSlot;
						portalConfigSlot.Type = partSlotType;
						portalConfigSlot.LocationX = Row.LocationX;
						portalConfigSlot.LocationY = Row.LocationY;
						portalConfigSlot.LocationZ = Row.LocationZ;
						portalConfigSlot.SizeX = Row.SizeX;
						portalConfigSlot.SizeY = Row.SizeY;
						portalConfigSlot.SizeZ = Row.SizeZ;
						portalConfigSlot.RotateX = Row.RotationX;
						portalConfigSlot.RotateY = Row.RotationY;
						portalConfigSlot.RotateZ = Row.RotationZ;
						portalConfigSlot.FlipX = Row.FlipX;
						portalConfigSlot.FlipY = Row.FlipY;
						portalConfigSlot.FlipZ = Row.FlipZ;

						pendingConfig.Slots.Add(portalConfigSlot);
					}
					// Parse a portal reference plane
					else
					{
						const TCHAR* planeStr = *Row.SlotType;
						if (ensureAlways((Row.SlotType.Len() >= 3) && (planeStr[0] == 'R')))
						{
							FPortalReferencePlane portalRefPlane;

							switch (planeStr[1])
							{
							case TEXT('X'):
								portalRefPlane.Axis = EAxis::X;
								break;
							case TEXT('Y'):
								portalRefPlane.Axis = EAxis::Y;
								break;
							case TEXT('Z'):
								portalRefPlane.Axis = EAxis::Z;
								break;
							default:
								break;
							}

							if (ensureAlways(portalRefPlane.Axis != EAxis::None))
							{
								TArray<FPortalReferencePlane> &axisReferencePlanes = pendingConfig.ReferencePlanes[(int32)portalRefPlane.Axis - 1];
								portalRefPlane.Index = axisReferencePlanes.Num();
								portalRefPlane.Name = FName(planeStr);

								ensureAlways(Row.LocationX.IsEmpty() != (portalRefPlane.Axis == EAxis::X));
								ensureAlways(Row.LocationY.IsEmpty() != (portalRefPlane.Axis == EAxis::Y));
								ensureAlways(Row.LocationZ.IsEmpty() != (portalRefPlane.Axis == EAxis::Z));

								const FString *valueStrPtr = nullptr;
								switch (portalRefPlane.Axis)
								{
								case EAxis::X:
									valueStrPtr = &Row.LocationX;
									break;
								case EAxis::Y:
									valueStrPtr = &Row.LocationY;
									break;
								case EAxis::Z:
									valueStrPtr = &Row.LocationZ;
									break;
								default:
									break;
								}

								// Either parse a fixed float value, or a variable expression value
								float planeFixedValue;
								if (LexTryParseString(planeFixedValue, **valueStrPtr))
								{
									portalRefPlane.FixedValue = Units::FUnitValue::WorldInches(planeFixedValue);
								}
								else
								{
									portalRefPlane.ValueExpression = *valueStrPtr;
								}

								axisReferencePlanes.Add(portalRefPlane);
							}
						}
					}
				}
			}
		});

		if (bReadingConfig)
		{
			pendingSet.Options.Add(pendingConfig);
		}

		if (pendingSet.Options.Num() != 0)
		{
			PortalConfigurationOptionSets.AddData(pendingSet);
			pendingSet.Options.Empty();
		}

	}

	/*
	Read Data Tables
	*/
	void ModumateObjectDatabase::ReadMeshData(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		AMeshes = DataCollection<FArchitecturalMesh>();

		data->ForeachRow<FMeshTableRow>(TEXT("FModumateMeshTableRow"),
			[this](const FName &Key, const FMeshTableRow &data)
			{
				FArchitecturalMesh mesh;

				// TODO: add lazy loading support
				if (data.AssetFilePath.IsAsset())
				{
					mesh.AssetPath = data.AssetFilePath;
					mesh.EngineMesh = Cast<UStaticMesh>(data.AssetFilePath.TryLoad());
					if (ensure(mesh.EngineMesh.IsValid()))
					{
						mesh.EngineMesh->AddToRoot();
					}

					mesh.Key = Key;
					AMeshes.AddData(mesh);
				}
			}
		);
	}

	void ModumateObjectDatabase::ReadAMaterialData(UDataTable *data)
	{
		if (!data)
		{
			return;
		}

		AMaterials = DataCollection<FArchitecturalMaterial>();

		data->ForeachRow<FMaterialTableRow>(TEXT("FMaterialTableRow"),
			[this](const FName &Key, const FMaterialTableRow &data)
		{
			if (ensure(data.EngineMaterialPath.IsValid() && data.EngineMaterialPath.IsAsset()))
			{
				FArchitecturalMaterial mat;
				mat.Key = *data.AssetName;
				mat.DisplayName = data.DisplayName;

				mat.EngineMaterial = Cast<UMaterialInterface>(data.EngineMaterialPath.TryLoad());
				if (ensure(mat.EngineMaterial.IsValid()))
				{
					mat.EngineMaterial->AddToRoot();
				}

				if (auto *baseColorPtr = NamedColors.GetData(*data.DefaultBaseColor))
				{
					mat.DefaultBaseColor = *baseColorPtr;
				}

				for (const auto &supportedBaseColorString : data.SupportedBaseColors)
				{
					if (auto *supportedColorPtr = NamedColors.GetData(*supportedBaseColorString))
					{
						mat.SupportedBaseColors.Add(*supportedColorPtr);
					}
				}

				mat.bSupportsColorPicker = data.SupportsRGBColorPicker;
				mat.UVScaleFactor = (data.TextureTilingSize > 0.0f) ? (1.0f / data.TextureTilingSize) : 1.0f;
				mat.HSVRangeWhenTiled = data.HSVRangeWhenTiled;

				AMaterials.AddData(mat);
			}
		});
	}

	void ModumateObjectDatabase::ReadFFEPartData(UDataTable *data)
	{
		if (data == nullptr)
		{
			return;
		}

		data->ForeachRow<FFFEPartTableRow>(TEXT("FFFEPartTableRow"),
			[this](const FName &Key, const FFFEPartTableRow &data)
		{
			FModumateObjectAssemblyLayer newLayer;
			newLayer.DatabaseKey = Key;
			newLayer.Thickness = Units::FUnitValue::WorldCentimeters(1.0f);

			newLayer.CodeName = "";

			newLayer.Mesh.AssetPath = data.AssetFilePath;
			newLayer.Mesh.Key = Key;
			newLayer.Mesh.EngineMesh = Cast<UStaticMesh>(newLayer.Mesh.AssetPath.TryLoad());
			if (ensureAlwaysMsgf(newLayer.Mesh.EngineMesh.IsValid(), TEXT("FFE Part %s missing mesh"), *(Key.ToString())))
			{
				newLayer.Mesh.EngineMesh->AddToRoot();
			}

			FSoftObjectPath simpleMeshPaths[] = {
				data.PerimeterMeshPath,
				data.InteriorMeshPath,
			};

			for (const FSoftObjectPath &simpleMeshPath : simpleMeshPaths)
			{
				if (simpleMeshPath.IsValid())
				{
					auto *simpleMeshData = Cast<USimpleMeshData>(simpleMeshPath.TryLoad());
					if (ensureAlwaysMsgf(simpleMeshData, TEXT("FFE Part %s missing simple mesh"),
						*(Key.ToString())))
					{
						FSimpleMeshRef poly = FSimpleMeshRef();
						poly.Key = FName(*simpleMeshPath.GetAssetName());
						poly.AssetPath = simpleMeshPath;
						poly.Asset = simpleMeshData;

						simpleMeshData->AddToRoot();
						newLayer.SimpleMeshes.Add(poly);
					}
				}
			}

			// Add lights to FFE layer
			if (data.LightConfigKey.Len() > 0)
			{
				newLayer.LightLocation = data.LightLocation;
				newLayer.LightRotation = data.LightRotation;
				const FLightConfiguration* lightConfigData = LightConfigs.GetData(*data.LightConfigKey);
				if (!ensureAlways(lightConfigData))
				{
					UE_LOG(LogTemp, Warning, TEXT("LightConfig %s missing data"), *data.LightConfigKey);
				}
				else
				{
					newLayer.LightConfiguration = *lightConfigData;
				}
			}
			FFEParts.AddData(newLayer);
		});
	}

	void ModumateObjectDatabase::ReadLightConfigData(UDataTable *data)
	{
		if (!ensureAlways(data))
		{
			return;
		}

		LightConfigs = DataCollection<FLightConfiguration>();

		data->ForeachRow<FLightTableRow>(TEXT("FModumateMeshTableRow"),
			[this](const FName &Key, const FLightTableRow &data)
		{
			FLightConfiguration lightdata;
			lightdata.LightIntensity = data.LightIntensity;
			lightdata.LightColor = data.LightColor;
			lightdata.bAsSpotLight = data.bAsSpotLight;

			// TODO: add lazy loading support
			if (data.LightProfilePath.IsAsset())
			{
				lightdata.LightProfile = Cast<UTextureLightProfile>(data.LightProfilePath.TryLoad());
			}
			lightdata.Key = Key;
			LightConfigs.AddData(lightdata);
		}
		);
	}

	void ModumateObjectDatabase::ReadFFEAssemblyData(UDataTable *data)
	{
		if (data == nullptr)
		{
			return;
		}
		data->ForeachRow<FFFEAssemblyTableRow>(TEXT("FFFEAssemblyTableRow"),
			[this](const FName &Key, const FFFEAssemblyTableRow &data)
		{
			FModumateObjectAssembly newAssembly;
			for (auto &ffeKey : data.FFEKeys)
			{
				const FModumateObjectAssemblyLayer *partData = FFEParts.GetData(*ffeKey);
				if (!ensureAlways(partData))
				{
					UE_LOG(LogTemp, Warning, TEXT("Wall %s missing layer data"), *ffeKey);
					continue;
				}
				newAssembly.Layers.Add(*partData);
			}
			newAssembly.DatabaseKey = Key;
			newAssembly.SetProperty(BIM::Parameters::Normal, data.NormalVector);
			newAssembly.SetProperty(BIM::Parameters::Tangent, data.TangentVector);
			newAssembly.SetProperty(BIM::Parameters::Name, data.DisplayName.ToString());
			newAssembly.ObjectType = EObjectType::OTFurniture;

			FFEAssemblies.AddData(newAssembly);
		});

	}

	void ModumateObjectDatabase::ReadColorData(UDataTable *data)
	{
		if (!data)
		{
			return;
		}

		data->ForeachRow<FColorTableRow>(TEXT("FColorTableRow"),
			[this](const FName &Key, const FColorTableRow &data)
		{
			FString keySuffix = Key.ToString();
			keySuffix.RemoveFromStart(data.Library.ToString());
			FColor colorValue = FColor::FromHex(data.Hex);

			FCustomColor namedColor = FCustomColor(FName(*keySuffix), MoveTemp(colorValue), data.Library, data.DisplayName);
			NamedColors.AddData(MoveTemp(namedColor));
		});
	}

	TArray<FString> ModumateObjectDatabase::GetDebugInfo()
	{
		// TODO: fill in debug info for console display
		TArray<FString> ret;
		return ret;
	}

	bool ModumateObjectDatabase::ParsePortalConfigDimensionSets(
		FName configKey, 
		const FString &typeString, 
		const TArray<FString> &setStrings, 
		TArray<FPortalConfigDimensionSet> &outSets)
	{
		FString configKeyStr = configKey.ToString();
		outSets.Reset();

		for (const FString &setString : setStrings)
		{
			FPortalConfigDimensionSet pendingSet;

			TArray<FString> pairStrings;
			setString.ParseIntoArray(pairStrings, TEXT(","), true);
			for (const FString &pairString : pairStrings)
			{
				const static FString pairDelim(TEXT("="));
				const static FString displayNameKey(TEXT("DisplayName"));

				FString keyStr, valueStr;
				if (pairString.Split(pairDelim, &keyStr, &valueStr))
				{
					float dimensionValueInches;

					if (keyStr.Equals(displayNameKey))
					{
						pendingSet.DisplayName = FText::FromString(valueStr);
						pendingSet.Key = FName(*FString::Printf(TEXT("%s_%s_%s"),
							*configKeyStr, *typeString, *valueStr));
					}
					else if (LexTryParseString(dimensionValueInches, *valueStr))
					{
						pendingSet.DimensionMap.Add(FName(*keyStr),
							Units::FUnitValue::WorldInches(dimensionValueInches));
					}
				}
			}

			if (!pendingSet.Key.IsNone() && (pendingSet.DimensionMap.Num() != 0))
			{
				outSets.Add(MoveTemp(pendingSet));
			}
		}

		return (outSets.Num() == setStrings.Num());
	}

	ModumateObjectDatabase::~ModumateObjectDatabase()
	{
	}

	/*
	Crafting DB
	*/

	template<class T>
	bool checkSubcategoryMetadata(
		const TArray<FName> &subcategories,
		const FString &optName,
		const DataCollection<T> &options)
	{
		bool ret = true;
		for (auto &kvp : options.DataMap)
		{
			if (!subcategories.Contains(kvp.Key))
			{
				UE_LOG(LogTemp, Warning, TEXT("Unidentified subcategory %s in meta-table %s"), *kvp.Key.ToString(), *optName);
				ret = false;
			}
		}
		return ret;
	}


	/*
	Data Access
	*/

	const FArchitecturalMaterial *ModumateObjectDatabase::GetArchitecturalMaterialByKey(const FName &key) const
	{
		return AMaterials.GetData(key);
	}

	const FCustomColor *ModumateObjectDatabase::GetCustomColorByKey(const FName &key) const
	{
		return NamedColors.GetData(key);
	}

	const FPortalPart *ModumateObjectDatabase::GetPortalPartByKey(const FName &key) const
	{
		return PortalParts.GetData(key);
	}

	const FSimpleMeshRef * ModumateObjectDatabase::GetSimpleMeshByKey(const FName &Key) const
	{
		return SimpleMeshes.GetData(Key);
	}

	const Modumate::FRoomConfiguration * ModumateObjectDatabase::GetRoomConfigByKey(const FName &Key) const
	{
		return RoomConfigurations.GetData(Key);
	}

	const FStaticIconTexture * ModumateObjectDatabase::GetStaticIconTextureByKey(const FName &Key) const
	{
		return StaticIconTextures.GetData(Key);
	}

	bool ModumateObjectDatabase::ParseColorFromField(FCustomColor &OutColor, const FString &Field)
	{
		static const FString NoColor(TEXT("N/A"));

		if (!Field.IsEmpty() && !Field.Equals(NoColor))
		{
			if (auto *namedColor = NamedColors.GetData(*Field))
			{
				OutColor = *namedColor;
			}
			else
			{
				OutColor.Color = FColor::FromHex(Field);
			}

			OutColor.bValid = true;
			return true;
		}

		return false;
	}

	void ModumateObjectDatabase::InitPresetManagerForNewDocument(FPresetManager &OutManager) const
	{
		if (ensureAlways(&OutManager != &PresetManager))
		{
			OutManager.InitAssemblyDBs();
			OutManager.CraftingPresetArray = PresetManager.CraftingPresetArray;
			OutManager.CraftingDecisionTrees = PresetManager.CraftingDecisionTrees;
			OutManager.CraftingBuiltins = PresetManager.CraftingBuiltins;
			OutManager.CraftingNodePresets = PresetManager.CraftingNodePresets;
			OutManager.DraftingNodePresets = PresetManager.DraftingNodePresets;
			OutManager.AssembliesByObjectType = PresetManager.AssembliesByObjectType;
			OutManager.KeyStore = PresetManager.KeyStore;

			//We don't copy all of the deprecated assemblies because they are the "marketplace" 
			//Instead, import the first entry in each as a default assembly available to the project
			for (auto &kvp : PresetManager.AssemblyDBs_DEPRECATED)
			{
				if (!UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(UModumateTypeStatics::ObjectTypeFromToolMode(kvp.Key)))
				{
					if (kvp.Value.DataMap.Num() == 0)
					{
						continue;
					}
					auto iterator = kvp.Value.DataMap.CreateConstIterator();
					DataCollection<FModumateObjectAssembly> &db = OutManager.AssemblyDBs_DEPRECATED.FindOrAdd(kvp.Key);
					db.AddData((*iterator).Value);
				}
			}
		}
	}

	void ModumateObjectDatabase::ReadMarketplace(UWorld *world)
	{
		FString defaultAssemblyPath = FPaths::Combine(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("NonUAssets")), TEXT("defaultAssemblies.mdmt"));

		PresetManager.InitAssemblyDBs();

		FModumateDocumentHeader docHeader;
		FMOIDocumentRecord docRecord;
		if (ensure(FModumateSerializationStatics::TryReadModumateDocumentRecord(defaultAssemblyPath, docHeader, docRecord)))
		{
			PresetManager.CraftingPresetArray = docRecord.CraftingPresetArray;
			PresetManager.CraftingNodePresets.FromDataRecords(docRecord.CraftingPresetArrayV2);
		}

		for (auto &ps : PresetManager.CraftingPresetArray)
		{
			ps.UpdatePropertiesFromArchive();
			ps.UpdatePresetNameFromProperties();
		}

		for (auto &car : docRecord.CustomAssemblies)
		{
			if (car.ObjectType == EObjectType::OTFurniture || UModumateObjectAssemblyStatics::ObjectTypeSupportsDDL2(car.ObjectType))
			{
				continue;
			}
			DataCollection<FModumateObjectAssembly> &obDB = PresetManager.AssemblyDBs_DEPRECATED.FindOrAdd(car.ToolMode);
			FModumateObjectAssembly newAsm;
			FModumateObjectAssembly::FromDataRecord_DEPRECATED(car, *this, PresetManager, newAsm);
			obDB.AddData(newAsm);
		}

		PresetManager.AssemblyDBs_DEPRECATED.Add(EToolMode::VE_PLACEOBJECT, FFEAssemblies);

		PresetManager.LoadObjectNodeSet(world);
	}

	void ModumateObjectDatabase::Init()
	{
	}

	void ModumateObjectDatabase::Shutdown()
	{
	}
}
