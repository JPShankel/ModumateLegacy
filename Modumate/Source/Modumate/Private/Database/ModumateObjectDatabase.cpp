// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateObjectDatabase.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Misc/FileHelper.h"
#include "Serialization/Csv/CsvParser.h"


FModumateDatabase::FModumateDatabase() {}	

FModumateDatabase::~FModumateDatabase() {}

void FModumateDatabase::Init() {}

void FModumateDatabase::Shutdown() {}

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

void FModumateDatabase::AddArchitecturalMesh(const FName& Key, const FString& Name, const FVector& InNativeSize, const FBox& InNineSliceBox, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FArchitecturalMesh mesh;
	mesh.AssetPath = AssetPath;
	mesh.NativeSize = InNativeSize;
	mesh.NineSliceBox = InNineSliceBox;
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

void FModumateDatabase::AddCustomColor(const FName& Key, const FString& Name, const FString& HexValue)
{
	FColor value = FColor::FromHex(HexValue);
	FCustomColor namedColor = FCustomColor(Key, MoveTemp(value), NAME_None, FText::FromString(Name));
	namedColor.CombinedKey = Key.ToString();
	NamedColors.AddData(MoveTemp(namedColor));
}

/*
This function is in development pending a complete data import/access plan
In the meantime, we read a manifest of CSV files and look for expected presets to populate tools
*/
void FModumateDatabase::ReadPresetData()
{
	FString manifestPath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMData");
	TArray<FString> errors;
	TArray<FName> starters;
	if (!ensureAlways(PresetManager.CraftingNodePresets.LoadCSVManifest(manifestPath, TEXT("BIMManifest.txt"), starters, errors) == ECraftingResult::Success))
	{
		return;
	}

	FString NCPString;
	FString NCPPath = manifestPath / TEXT("NCPTable.csv");
	if (!ensureAlways(FFileHelper::LoadFileToString(NCPString, *NCPPath)))
	{
		return;
	}

	FCsvParser NCPParsed(NCPString);
	const FCsvParser::FRows &NCPRows = NCPParsed.GetRows();

	/*
	Presets make reference to raw assets, like meshes and engine materials, stored in the object database
	and keyed by a given ID. The NCP table indicates which preset types have AssetPath/AssetID pairs to be
	loaded and bound to the object database. This map binds tag paths to asset load functions.

	The map is stored as an array of pairs instead of an actual map because tags are checked with partial matches

	Future plan: these relations will be used to build a SQL database

	*/

	typedef TFunction<void(const FBIMPreset &Preset)> FAddAssetFunction;
	typedef TPair<FBIMTagPath, FAddAssetFunction> FAddAssetPath;
	TArray<FAddAssetPath> assetTargetPaths;

	FAddAssetFunction addColor = [this](const FBIMPreset &Preset)
	{
		FString hexValue = Preset.GetProperty(BIMPropertyNames::HexValue);
		FString colorName = Preset.GetProperty(BIMPropertyNames::Name);
		AddCustomColor(Preset.PresetID, colorName, hexValue);
	};

	FAddAssetFunction addMesh = [this](const FBIMPreset &Preset)
	{
		FString assetPath = Preset.GetScopedProperty(EBIMValueScope::Mesh, BIMPropertyNames::AssetPath);

		// TODO: to be replaced with typesafe properties
		if (assetPath.Len() != 0)
		{
			FVector nativeSize(
				Preset.GetProperty(TEXT("NativeSizeX")), 
				Preset.GetProperty(TEXT("NativeSizeY")), 
				Preset.GetProperty(TEXT("NativeSizeZ"))
			);

			FBox nineSlice(
				FVector(Preset.GetProperty(TEXT("SliceX1")),
					Preset.GetProperty(TEXT("SliceY1")),
					Preset.GetProperty(TEXT("SliceZ1"))),

				FVector(Preset.GetProperty(TEXT("SliceX2")),
					Preset.GetProperty(TEXT("SliceY2")),
					Preset.GetProperty(TEXT("SliceZ2")))
			);

			FString name = Preset.GetProperty(BIMPropertyNames::Name);
			AddArchitecturalMesh(Preset.PresetID, name, nativeSize, nineSlice, assetPath);
		}
	};

	FAddAssetFunction addRawMaterial = [this](const FBIMPreset &Preset)
	{
		FString assetPath = Preset.GetProperty(BIMPropertyNames::AssetPath);
		if (assetPath.Len() != 0)
		{
			FString matName = Preset.GetProperty(BIMPropertyNames::Name);
			AddArchitecturalMaterial(Preset.PresetID, matName, assetPath);
		}
	};

	FAddAssetFunction addMaterial = [this](const FBIMPreset &Preset)
	{
		FName rawMaterial;
		for (auto& cp : Preset.ChildPresets)
		{
			const FBIMPreset* childPreset = PresetManager.CraftingNodePresets.Presets.Find(cp.PresetID);
			if (childPreset != nullptr && childPreset->NodeScope == EBIMValueScope::RawMaterial)
			{
				rawMaterial = cp.PresetID;
				break;
			}
		}

		if (!rawMaterial.IsNone())
		{
			const FBIMPreset* preset = PresetManager.CraftingNodePresets.Presets.Find(rawMaterial);
			if (preset != nullptr)
			{
				FString assetPath = preset->GetProperty(BIMPropertyNames::AssetPath);
				FString matName = Preset.GetProperty(BIMPropertyNames::Name);
				AddArchitecturalMaterial(Preset.PresetID, matName, assetPath);
			}
		}
	};

	FAddAssetFunction addProfile = [this](const FBIMPreset &Preset)
	{
		FString assetPath = Preset.GetProperty(BIMPropertyNames::AssetPath);
		if (assetPath.Len() != 0)
		{
			FString name = Preset.GetProperty(BIMPropertyNames::Name);
			AddSimpleMesh(Preset.PresetID, name, assetPath);
		}
	};

	/*
	Find the columns containing object type and asset destination (which db) for each preset type
	*/
	const FString objectTypeS = TEXT("ObjectType");
	int32 objectTypeI = -1;

	const FString assetTypeS = TEXT("AssetType");
	int32 assetTypeI = -1;

	for (int32 column = 0; column < NCPRows[0].Num(); ++column)
	{
		if (objectTypeS.Equals(NCPRows[0][column]))
		{
			objectTypeI = column;
		}
		if (assetTypeS.Equals(NCPRows[0][column]))
		{
			assetTypeI = column;
		}
	}

	/*
	Now, for each row, record what kind of object it creates (store in objectPaths) and what if any assets it loads (assetTargetPaths)
	*/
	const FString colorTag = TEXT("Color");
	const FString meshTag = TEXT("Mesh");
	const FString profileTag = TEXT("Profile");
	const FString rawMaterialTag = TEXT("RawMaterial");
	const FString materialTag = TEXT("Material");

	typedef TFunction<void(const FBIMTagPath &TagPath)> FAssetTargetAssignment;
	TMap<FString, FAssetTargetAssignment> assetTargetMap;

	assetTargetMap.Add(colorTag, [addColor, &assetTargetPaths](const FBIMTagPath &TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addColor)); });
	assetTargetMap.Add(meshTag, [addMesh, &assetTargetPaths](const FBIMTagPath &TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addMesh)); });
	assetTargetMap.Add(profileTag, [addProfile, &assetTargetPaths](const FBIMTagPath &TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addProfile)); });
	assetTargetMap.Add(rawMaterialTag, [addRawMaterial, &assetTargetPaths](const FBIMTagPath &TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addRawMaterial)); });
	assetTargetMap.Add(materialTag, [addMaterial, &assetTargetPaths](const FBIMTagPath &TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addMaterial)); });

	typedef TPair<FBIMTagPath, EObjectType> FObjectPathRef;
	TArray<FObjectPathRef> objectPaths;

	for (int32 row = 1; row < NCPRows.Num(); ++row)
	{
		// Get the object type cell and add it to the object map if applicable
		FString cell = NCPRows[row][objectTypeI];
		if (!cell.IsEmpty())
		{
			EObjectType ot = EnumValueByString(EObjectType, cell);
			FBIMTagPath tagPath;
			FString tagStr = NCPRows[row][0];
			tagPath.FromString(FString(NCPRows[row][0]).Replace(TEXT(" "),TEXT("")));
			objectPaths.Add(FObjectPathRef(tagPath, ot));
		}

		// Add asset loader if applicable
		cell = NCPRows[row][assetTypeI];
		if (!cell.IsEmpty())
		{
			FAssetTargetAssignment *assignment = assetTargetMap.Find(cell);
			if (assignment != nullptr)
			{
				FString tagStr = NCPRows[row][0];
				FBIMTagPath tagPath;
				tagPath.FromString(tagStr);
				(*assignment)(tagPath);
			}
		}
	}

	/*
	Temp hack: until cabinets and portals are reintroduced, we match the "stubby" representations
	*/
	const TArray<TPair<FString, EObjectType>> stubbies = {
		TPair <FString,EObjectType>(TEXT("4RiggedAssembly-->Door"),EObjectType::OTDoor),
		TPair <FString,EObjectType>(TEXT("4RiggedAssembly-->Window"),EObjectType::OTWindow),
		TPair <FString,EObjectType>(TEXT("2Part0Slice-->FF&E"),EObjectType::OTFurniture),
		TPair <FString,EObjectType>(TEXT("4RiggedAssembly-->CabinetFace"),EObjectType::OTCabinet),
		TPair <FString,EObjectType>(TEXT("4RiggedAssembly-->Countertop"),EObjectType::OTCountertop) };

	for (auto& stubby : stubbies)
	{
		FBIMTagPath tempTag;
		tempTag.FromString(stubby.Key);
		objectPaths.Add(FObjectPathRef(tempTag, stubby.Value));
		assetTargetPaths.Add(FAddAssetPath(tempTag, addMesh));
	}

	/*
	For every preset, load its dependent assets (if any) and set its object type based on tag path
	*/
	TSet<EObjectType> gotDefault;
	for (auto &kvp : PresetManager.CraftingNodePresets.Presets)
	{
		// Load assets (mesh, material, profile or color)
		for (auto& assetFunc : assetTargetPaths)
		{
			if (kvp.Value.MyTagPath.MatchesPartial(assetFunc.Key))
			{
				assetFunc.Value(kvp.Value);
			}
		}

		// Set object type
		EObjectType *ot = nullptr;
		for (auto& tagPath : objectPaths)
		{
			if (tagPath.Key.MatchesPartial(kvp.Value.MyTagPath))
			{
				ot = &tagPath.Value;
				break;
			}
		}

		if (ot == nullptr)
		{
			continue;
		}

		kvp.Value.ObjectType = *ot;

		// "Stubby" assemblies used for portals are temporary, don't have starter codes, so add 'em all
		switch (*ot)
		{
			case EObjectType::OTDoor:
			case EObjectType::OTWindow:
			case EObjectType::OTFurniture:
			case EObjectType::OTCabinet:
			case EObjectType::OTCountertop:
				starters.Add(kvp.Key);
			break;
		}
	}

	/*
	Now build every preset that was returned in 'starters' by the manifest parse and add it to the project
	*/
	for (auto &starter : starters)
	{
		const FBIMPreset *preset = PresetManager.CraftingNodePresets.Presets.Find(starter);

		if (!ensureAlways(preset != nullptr && preset->ObjectType != EObjectType::OTNone))
		{
			continue;
		}

		FBIMAssemblySpec outSpec;
		outSpec.FromPreset(*this, PresetManager.CraftingNodePresets, starter);
		outSpec.ObjectType = preset->ObjectType;PresetManager.UpdateProjectAssembly(outSpec);

		// TODO: default assemblies added to allow interim loading during assembly refactor, to be eliminated
		if (!gotDefault.Contains(outSpec.ObjectType))
		{
			gotDefault.Add(outSpec.ObjectType);
			outSpec.RootPreset = TEXT("default");
			PresetManager.UpdateProjectAssembly(outSpec);
		}
	}
}

TArray<FString> FModumateDatabase::GetDebugInfo()
{
	// TODO: fill in debug info for console display
	TArray<FString> ret;
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

void FModumateDatabase::InitPresetManagerForNewDocument(FPresetManager &OutManager) const
{
	if (ensureAlways(&OutManager != &PresetManager))
	{
		OutManager = PresetManager;
	}
}


