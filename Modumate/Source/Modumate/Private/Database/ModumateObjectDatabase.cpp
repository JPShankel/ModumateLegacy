// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateObjectDatabase.h"

#include "Components/StaticMeshComponent.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "Engine/StaticMeshActor.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Database/ModumateDataTables.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"


template<class T, class O, class OS>
void ReadOptionSet(
	UDataTable *data,
	std::function<bool(const T &row, O &ot)> readOptionData,
	std::function<void(const OS &o)> addOptionSet
)
{
	if (!ensureAlways(data))
	{
		return;
	}

	OS optionSet;
	TArray<FString> supportedSubcategories;

	data->ForeachRow<T>(TEXT("OPTIONSET"),
		[&optionSet, &supportedSubcategories, readOptionData, addOptionSet](const FName &Key, const T &row)
	{
		if (row.SupportedSubcategories.Num() > 0)
		{
			if (optionSet.Options.Num() > 0)
			{
				for (auto &sc : supportedSubcategories)
				{
					optionSet.Key = *sc;
					addOptionSet(optionSet);
				}
			}
			optionSet.Options.Empty();
			supportedSubcategories = row.SupportedSubcategories;
		}

		O option;
		option.Key = Key;
		if (readOptionData(row, option))
		{
			optionSet.Options.Add(option);
		}
	});

	if (optionSet.Options.Num() > 0)
	{
		for (auto &sc : supportedSubcategories)
		{
			optionSet.Key = *sc;
			addOptionSet(optionSet);
		}
	}
}


FModumateDatabase::FModumateDatabase() {}	

void FModumateDatabase::ReadCraftingPortalPartOptionSet(UDataTable *data)
{
	ReadOptionSet<
		FPortalPartOptionsSetDataTable, 
		Modumate::FCraftingPortalPartOption,
		Modumate::FCraftingPortalPartOptionSet>
	(
		data, 
		[this](const FPortalPartOptionsSetDataTable &row, Modumate::FCraftingPortalPartOption &po)
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
		[this](const Modumate::FCraftingPortalPartOptionSet &pos)
		{
			PortalPartOptionSets.AddData(pos);
		}
	);
}

void FModumateDatabase::ReadCraftingPatternOptionSet(UDataTable *data)
{
	ReadOptionSet<FPatternOptionSetDataTable,
		Modumate::FCraftingOptionBase,
		Modumate::FCraftingOptionSet>(data,
			[this](const FPatternOptionSetDataTable &row, Modumate::FCraftingOptionBase &option)
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
						Modumate::Expression::Evaluate(patternExprVars, patternWidthExpr, patternExtentsValue.X) &&
						Modumate::Expression::Evaluate(patternExprVars, patternHeightExpr, patternExtentsValue.Y),
						TEXT("Pattern %s failed to evaluate module extents expression: %s"),
						*option.DisplayName.ToString(), *pattern.ParameterizedExtents);
				}

				int32 numModuleTiles = pattern.ParameterizedModuleDimensions.Num();
				for (int32 moduleInstIdx = 0; moduleInstIdx < numModuleTiles; ++moduleInstIdx)
				{
					auto &moduleInstData = pattern.ParameterizedModuleDimensions[moduleInstIdx];
					float moduleX, moduleY, moduleW, moduleH;

					ensureAlwaysMsgf(
						Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleXExpr, moduleX) &&
						Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleYExpr, moduleY) &&
						Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleWidthExpr, moduleW) &&
						Modumate::Expression::Evaluate(patternExprVars, moduleInstData.ModuleHeightExpr, moduleH),
						TEXT("Pattern %s failed to evaluate module #%d expression(s)!"),
						*option.DisplayName.ToString(), moduleInstIdx);
				}

				return true;
			},
			[this](const Modumate::FCraftingOptionSet &os)
			{
				const Modumate::FCraftingOptionSet *optionSet = PatternOptionSets.GetData(os.Key);
				if (optionSet != nullptr)
				{
					Modumate::FCraftingOptionSet newSet = *optionSet;
					newSet.Options.Append(os.Options);
					PatternOptionSets.AddData(newSet);
				}
				else
				{
					PatternOptionSets.AddData(os);
				}
			});
}

void FModumateDatabase::ReadCraftingProfileOptionSet(UDataTable *data)
{
	if (!ensureAlways(data))
	{
		return;
	}

	ReadOptionSet<
		FProfileOptionSetTableRow,
		Modumate::FCraftingOptionBase,
		Modumate::FCraftingOptionSet>(data,
			[this](const FProfileOptionSetTableRow &row, Modumate::FCraftingOptionBase &option)
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
	[this](const Modumate::FCraftingOptionSet &optionSet)
	{
		ProfileOptionSets.AddData(optionSet);
	});
}

void FModumateDatabase::ReadRoomConfigurations(UDataTable *data)
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
			Modumate::FRoomConfiguration newRow;
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
void FModumateDatabase::ReadPortalPartData(UDataTable *data)
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
void FModumateDatabase::ReadPortalConfigurationData(UDataTable *data)
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
	Modumate::FPortalAssemblyConfigurationOption pendingConfig;
	Modumate::FPortalAssemblyConfigurationOptionSet pendingSet;
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
			pendingConfig = Modumate::FPortalAssemblyConfigurationOption();
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
				Modumate::FPortalAssemblyConfigurationSlot newPortalSlot;
				EPortalSlotType partSlotType = EPortalSlotType::None;

				// Parse a regular portal part
				if (TryEnumValueByString(EPortalSlotType, Row.SlotType, partSlotType))
				{
					Modumate::FPortalAssemblyConfigurationSlot portalConfigSlot;
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
						Modumate::FPortalReferencePlane portalRefPlane;

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
							TArray<Modumate::FPortalReferencePlane> &axisReferencePlanes = pendingConfig.ReferencePlanes[(int32)portalRefPlane.Axis - 1];
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
								portalRefPlane.FixedValue = Modumate::Units::FUnitValue::WorldInches(planeFixedValue);
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

void FModumateDatabase::AddSimpleMesh(const FName& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FSimpleMeshRef mesh;

	mesh.Key = Key;
	mesh.AssetPath = AssetPath;

	mesh.Asset = Cast<USimpleMeshData>(AssetPath.TryLoad());
	if (ensureAlways(mesh.Asset.IsValid()))
	{
		mesh.Asset->AddToRoot();
	}

	SimpleMeshes.AddData(mesh);
}

void FModumateDatabase::AddArchitecturalMesh(const FName& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FArchitecturalMesh mesh;
	mesh.AssetPath = AssetPath;
	mesh.Key = Key;

	mesh.EngineMesh = Cast<UStaticMesh>(AssetPath.TryLoad());
	if (ensureAlways(mesh.EngineMesh.IsValid()))
	{
		mesh.EngineMesh->AddToRoot();
	}

	AMeshes.AddData(mesh);
}

void FModumateDatabase::AddArchitecturalMaterial(const FName& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FArchitecturalMaterial mat;
	mat.Key = Key;
	mat.DisplayName = FText::FromString(Name);

	mat.EngineMaterial = Cast<UMaterialInterface>(AssetPath.TryLoad());
	if (ensure(mat.EngineMaterial.IsValid()))
	{
		mat.EngineMaterial->AddToRoot();
	}

	mat.UVScaleFactor = 1;
	AMaterials.AddData(mat);
}

/*
This function is in development pending a complete data import/access plan
In the meantime, we read a manifest of CSV files and look for expected presets to populate tools
*/
void FModumateDatabase::ReadPresetData()
{
	FString manifestPath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMData");
	TArray<FString> errors;
	if (!ensureAlways(PresetManager.CraftingNodePresets.LoadCSVManifest(manifestPath, TEXT("BIMManifest.txt"), errors) == ECraftingResult::Success))
	{
		return;
	}

	Modumate::BIM::FCraftingTreeNodeType *layerNode = PresetManager.CraftingNodePresets.NodeDescriptors.Find(TEXT("2Layer0D"));
	layerNode->Scope = EBIMValueScope::Layer;

	FName rawMaterialType = TEXT("0RawMaterial");
	FName layeredType = TEXT("4LayeredAssembly");
	FName riggedType = TEXT("0StubbyPortal");
	FName ffeType = TEXT("0StubbyFFE");
	FName beamColumnType = TEXT("2ExtrudedProfile");
	FName profileType = TEXT("0Profile");

	for (auto& kvp : PresetManager.CraftingNodePresets.Presets)
	{
		if (kvp.Value.NodeType == rawMaterialType)
		{
			FString matName = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Preset, Modumate::BIM::Parameters::Name);
			FName matKey = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Node, Modumate::BIM::Parameters::MaterialKey);
			FString assetPathStr = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Node, Modumate::BIM::Parameters::EngineMaterial);
			AddArchitecturalMaterial(matKey, matName, assetPathStr);
		}
		if (kvp.Value.NodeType == riggedType || kvp.Value.NodeType == ffeType)
		{
			FString assetPath = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Layer, Modumate::BIM::Parameters::AssetID);
			FString name = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Preset, Modumate::BIM::Parameters::Name);
			AddArchitecturalMesh(*name, name, assetPath);
		}
		if (kvp.Value.NodeType == profileType)
		{
			FString assetPath = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Node, Modumate::BIM::Parameters::Mesh);
			if (assetPath.Len() != 0)
			{
				FString name = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Preset, Modumate::BIM::Parameters::Name);
				FString key = kvp.Value.Properties.GetProperty(Modumate::BIM::EScope::Node, Modumate::BIM::Parameters::ProfileKey);
				AddSimpleMesh(*key, name, assetPath);
			}
		}
	}

	/*
	TODO: prior to refactor, we must produce assemblies from presets with known tag paths...to be removed
	*/
	TMap<FString, EObjectType> objectMap;
	objectMap.Add(FString(TEXT("4LayeredAssembly-->Wall")), EObjectType::OTWallSegment);
	objectMap.Add(FString(TEXT("4LayeredAssembly-->Floor")), EObjectType::OTFloorSegment);
	objectMap.Add(FString(TEXT("4LayeredAssembly-->Roof")), EObjectType::OTRoofFace);
	objectMap.Add(FString(TEXT("4LayeredAssembly-->Finish")), EObjectType::OTFinish);
	objectMap.Add(FString(TEXT("4LayeredAssembly-->Countertop")), EObjectType::OTCountertop);

	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->ConcreteRectangular")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->ConcreteRound")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->SteelC")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->SteelHSS")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->SteelL")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->SteelTube")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->SteelW")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->SteelWT")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->StoneRectangular")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->StoneRound")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->WoodRectangular")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("2ExtrudedProfile-->Beam/Column-->WoodRound")),EObjectType::OTStructureLine);
	objectMap.Add(FString(TEXT("4RiggedAssembly-->Door-->Swing")), EObjectType::OTDoor);
	objectMap.Add(FString(TEXT("4RiggedAssembly-->Window-->Fixed")), EObjectType::OTWindow);
	objectMap.Add(FString(TEXT("4RiggedAssembly-->Window-->Hung")), EObjectType::OTWindow);

	objectMap.Add(FString(TEXT("2Part0Slice-->FF&E")), EObjectType::OTFurniture);

	TSet<EObjectType> gotDefault;
	for (auto &kvp : PresetManager.CraftingNodePresets.Presets)
	{
		if (kvp.Value.NodeType == riggedType || kvp.Value.NodeType == layeredType || kvp.Value.NodeType == ffeType || kvp.Value.NodeType == beamColumnType)
		{
			FString myPath;
			kvp.Value.MyTagPath.ToString(myPath);

			EObjectType *ot = objectMap.Find(myPath);
			if (ot == nullptr)
			{
				continue;
			}

			kvp.Value.ObjectType = *ot;

			if (!PresetManager.StarterPresetsByObjectType.Contains(*ot))
			{
				PresetManager.StarterPresetsByObjectType.Add(*ot, kvp.Key);
			}

			FBIMAssemblySpec outSpec;
			PresetManager.PresetToSpec(kvp.Key, outSpec);
			outSpec.ObjectType = *ot;
			UModumateObjectAssemblyStatics::DoMakeAssembly(*this, PresetManager, outSpec, outSpec.CachedAssembly);
			PresetManager.UpdateProjectAssembly(outSpec);

			// TODO: default assemblies added to allow interim loading during assembly refactor, to be eliminated
			if (!gotDefault.Contains(outSpec.ObjectType))
			{
				gotDefault.Add(outSpec.ObjectType);
				outSpec.RootPreset = TEXT("default");
				PresetManager.UpdateProjectAssembly(outSpec);
			}
		}
	}
	ensureAlways(errors.Num() == 0);
}

/*
Read Data Tables
*/
void FModumateDatabase::ReadMeshData(UDataTable *data)
{
	if (!ensureAlways(data))
	{
		return;
	}

	AMeshes = TModumateDataCollection<FArchitecturalMesh>();

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

void FModumateDatabase::ReadLightConfigData(UDataTable *data)
{
	if (!ensureAlways(data))
	{
		return;
	}

	LightConfigs = TModumateDataCollection<FLightConfiguration>();

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

void FModumateDatabase::ReadColorData(UDataTable *data)
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

TArray<FString> FModumateDatabase::GetDebugInfo()
{
	// TODO: fill in debug info for console display
	TArray<FString> ret;
	return ret;
}

bool FModumateDatabase::ParsePortalConfigDimensionSets(
	FName configKey, 
	const FString &typeString, 
	const TArray<FString> &setStrings, 
	TArray<Modumate::FPortalConfigDimensionSet> &outSets)
{
	FString configKeyStr = configKey.ToString();
	outSets.Reset();

	for (const FString &setString : setStrings)
	{
		Modumate::FPortalConfigDimensionSet pendingSet;

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
						Modumate::Units::FUnitValue::WorldInches(dimensionValueInches));
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

FModumateDatabase::~FModumateDatabase()
{
}

/*
Crafting DB
*/

template<class T>
bool checkSubcategoryMetadata(
	const TArray<FName> &subcategories,
	const FString &optName,
	const TModumateDataCollection<T> &options)
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

const FArchitecturalMaterial *FModumateDatabase::GetArchitecturalMaterialByKey(const FName& Key) const
{
	return AMaterials.GetData(Key);
}

const FArchitecturalMesh* FModumateDatabase::GetArchitecturalMeshByKey(const FName& Key) const
{
	return AMeshes.GetData(Key);
}

const FCustomColor *FModumateDatabase::GetCustomColorByKey(const FName& Key) const
{
	return NamedColors.GetData(Key);
}

const FPortalPart *FModumateDatabase::GetPortalPartByKey(const FName& Key) const
{
	return PortalParts.GetData(Key);
}

const FSimpleMeshRef* FModumateDatabase::GetSimpleMeshByKey(const FName& Key) const
{
	return SimpleMeshes.GetData(Key);
}

const Modumate::FRoomConfiguration * FModumateDatabase::GetRoomConfigByKey(const FName &Key) const
{
	return RoomConfigurations.GetData(Key);
}

const FStaticIconTexture * FModumateDatabase::GetStaticIconTextureByKey(const FName &Key) const
{
	return StaticIconTextures.GetData(Key);
}

bool FModumateDatabase::ParseColorFromField(FCustomColor &OutColor, const FString &Field)
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

void FModumateDatabase::InitPresetManagerForNewDocument(FPresetManager &OutManager) const
{
	if (ensureAlways(&OutManager != &PresetManager))
	{
		OutManager.CraftingNodePresets = PresetManager.CraftingNodePresets;
		OutManager.DraftingNodePresets = PresetManager.DraftingNodePresets;
		OutManager.AssembliesByObjectType = PresetManager.AssembliesByObjectType;
		OutManager.KeyStore = PresetManager.KeyStore;
		OutManager.StarterPresetsByObjectType = PresetManager.StarterPresetsByObjectType;
	}
}

void FModumateDatabase::Init()
{
}

void FModumateDatabase::Shutdown()
{
}

