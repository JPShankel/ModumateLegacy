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

	FCraftingTreeNodeType *layerNode = PresetManager.CraftingNodePresets.NodeDescriptors.Find(TEXT("2Layer0D"));
	layerNode->Scope = EBIMValueScope::Layer;

	FName rawMaterialType = TEXT("0RawMaterial");
	FName layeredType = TEXT("4LayeredAssembly");
	FName riggedType = TEXT("0StubbyPortal");
	FName ffeType = TEXT("0StubbyFFE");
	FName beamColumnType = TEXT("2ExtrudedProfile");
	FName profileType = TEXT("0Profile");
	FName cabinetType = TEXT("0StubbyCabinets");
	FName countertopType = TEXT("0StubbyCountertops");

	TSet<FName> assemblyTypes = { rawMaterialType, layeredType, riggedType, ffeType, beamColumnType, profileType, cabinetType, countertopType };

	for (auto& kvp : PresetManager.CraftingNodePresets.Presets)
	{
		if (kvp.Value.NodeType == rawMaterialType)
		{
			FString matName = kvp.Value.Properties.GetProperty(EBIMValueScope::Preset, BIMPropertyNames::Name);
			FName matKey = kvp.Value.Properties.GetProperty(EBIMValueScope::Node, BIMPropertyNames::MaterialKey);
			FString assetPathStr = kvp.Value.Properties.GetProperty(EBIMValueScope::Node, BIMPropertyNames::EngineMaterial);
			AddArchitecturalMaterial(matKey, matName, assetPathStr);
		}
		else
		if (kvp.Value.NodeType == cabinetType || kvp.Value.NodeType == countertopType)
		{
			FString assetPath = kvp.Value.Properties.GetProperty(EBIMValueScope::Node, BIMPropertyNames::AssetID);
			FString name = kvp.Value.Properties.GetProperty(EBIMValueScope::Preset, BIMPropertyNames::Name);
			AddArchitecturalMesh(*name, name, assetPath);
		}
		else
		if (kvp.Value.NodeType == riggedType || kvp.Value.NodeType == ffeType)
		{
			FString assetPath = kvp.Value.Properties.GetProperty(EBIMValueScope::Layer, BIMPropertyNames::AssetID);
			FString name = kvp.Value.Properties.GetProperty(EBIMValueScope::Preset, BIMPropertyNames::Name);
			AddArchitecturalMesh(*name, name, assetPath);
		}
		else
		if (kvp.Value.NodeType == profileType)
		{
			FString assetPath = kvp.Value.Properties.GetProperty(EBIMValueScope::Node, BIMPropertyNames::Mesh);
			if (assetPath.Len() != 0)
			{
				FString name = kvp.Value.Properties.GetProperty(EBIMValueScope::Preset, BIMPropertyNames::Name);
				FString key = kvp.Value.Properties.GetProperty(EBIMValueScope::Node, BIMPropertyNames::ProfileKey);
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
	objectMap.Add(FString(TEXT("4RiggedAssembly-->CabinetFace")), EObjectType::OTCabinet);
	objectMap.Add(FString(TEXT("4LayeredAssembly-->Countertop")), EObjectType::OTCountertop);

	TSet<EObjectType> gotDefault;
	for (auto &kvp : PresetManager.CraftingNodePresets.Presets)
	{
		if (assemblyTypes.Contains(kvp.Value.NodeType))
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
			outSpec.FromPreset(PresetManager.CraftingNodePresets, kvp.Key);
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

