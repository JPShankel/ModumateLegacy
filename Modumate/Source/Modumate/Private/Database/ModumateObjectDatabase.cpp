// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateObjectDatabase.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "Misc/FileHelper.h"
#include "Serialization/Csv/CsvParser.h"
#include "DocumentManagement/ModumateSerialization.h"

FModumateDatabase::FModumateDatabase() {}	

FModumateDatabase::~FModumateDatabase() {}

void FModumateDatabase::Init() 
{
	ManifestDirectoryPath = FPaths::ProjectContentDir() / TEXT("NonUAssets") / TEXT("BIMData");
}

void FModumateDatabase::Shutdown() {}

void FModumateDatabase::AddSimpleMesh(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath)
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

void FModumateDatabase::AddArchitecturalMesh(const FGuid& Key, const FString& Name, const FString& InNamedParams, const FVector& InNativeSize, const FBox& InNineSliceBox, const FSoftObjectPath& AssetPath)
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
	mesh.ReadNamedDimensions(InNamedParams);

	mesh.EngineMesh = Cast<UStaticMesh>(AssetPath.TryLoad());
	if (ensureAlways(mesh.EngineMesh.IsValid()))
	{
		mesh.EngineMesh->AddToRoot();
	}

	AMeshes.AddData(mesh);
}

void FModumateDatabase::AddArchitecturalMaterial(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FArchitecturalMaterial mat;
	mat.Key = Key;
	mat.DisplayName = FText::FromString(Name);

	mat.EngineMaterial = Cast<UMaterialInterface>(AssetPath.TryLoad());
	if (ensureAlways(mat.EngineMaterial.IsValid()))
	{
		mat.EngineMaterial->AddToRoot();
	}

	mat.UVScaleFactor = 1;
	AMaterials.AddData(mat);
}


/*
* TODO: Refactor for FArchive binary format
*/
static constexpr TCHAR* BIMCacheRecordField = TEXT("BIMCacheRecord");
static constexpr TCHAR* BIMManifestFileName = TEXT("BIMManifest.txt");

bool FModumateDatabase::ReadBIMCache(const FString& CacheFile, FModumateBIMCacheRecord& OutCache)
{
	FString cacheFile = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), CacheFile);
	if (!FPaths::FileExists(cacheFile))
	{
		return false;
	}

	// In development, automatically update the cache if the source data is younger
#if WITH_EDITOR
	FString manifestFilePath = FPaths::Combine(*ManifestDirectoryPath, BIMManifestFileName);
	FDateTime cacheDate = IFileManager::Get().GetTimeStamp(*cacheFile);

	FDateTime manifestDate = IFileManager::Get().GetTimeStamp(*manifestFilePath);

	if (manifestDate > cacheDate)
	{
		return false;
	}

	TArray<FString> fileList;
	if (FFileHelper::LoadFileToStringArray(fileList, *manifestFilePath))
	{
		for (auto& file : fileList)
		{
			FString bimFile = ManifestDirectoryPath / *file;
			FDateTime bimDate = IFileManager::Get().GetTimeStamp(*bimFile);
			if (bimDate > cacheDate)
			{
				return false;
			}
		}
	}
#endif

	FString jsonString;
	if (!FFileHelper::LoadFileToString(jsonString, *cacheFile))
	{
		return false;
	}

	auto JsonReader = TJsonReaderFactory<>::Create(jsonString);

	TSharedPtr<FJsonObject> FileJson;
	if (!FJsonSerializer::Deserialize(JsonReader, FileJson) || !FileJson.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* cacheOb;
	if (!FileJson->TryGetObjectField(BIMCacheRecordField, cacheOb))
	{
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectToUStruct<FModumateBIMCacheRecord>(cacheOb->ToSharedRef(), &OutCache))
	{
		return false;
	}

	if (OutCache.Version < BIMCacheCurrentVersion)
	{
		return false;
	}

	return true;
}

bool FModumateDatabase::WriteBIMCache(const FString& CacheFile, const FModumateBIMCacheRecord& InCache) const
{
	FString cacheFile = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), CacheFile);

	TSharedPtr<FJsonObject> jsonOb = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> cacheOb = FJsonObjectConverter::UStructToJsonObject<FModumateBIMCacheRecord>(InCache);
	jsonOb->SetObjectField(BIMCacheRecordField, cacheOb);

	FString ProjectJsonString;
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&ProjectJsonString);

	return FJsonSerializer::Serialize(jsonOb.ToSharedRef(), JsonStringWriter) && FFileHelper::SaveStringToFile(ProjectJsonString, *cacheFile);
}

void FModumateDatabase::AddStaticIconTexture(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath)
{
	if (!ensureAlways(AssetPath.IsAsset() && AssetPath.IsValid()))
	{
		return;
	}

	FStaticIconTexture staticTexture;

	staticTexture.Key = Key;
	staticTexture.DisplayName = FText::FromString(Name);
	staticTexture.Texture = Cast<UTexture2D>(AssetPath.TryLoad());
	if (staticTexture.Texture.IsValid())
	{
		staticTexture.Texture->AddToRoot();
		StaticIconTextures.AddData(staticTexture);
	}
}

/*
This function is in development pending a complete data import/access plan
In the meantime, we read a manifest of CSV files and look for expected presets to populate tools
*/
void FModumateDatabase::ReadPresetData()
{
	// Parts may have unique values like "JambLeftSizeX" or "PeepholeDistanceZ"
	// If a part is expected to have one of these values but doesn't, we provide defaults here
	TArray<FString> partDefaultVals;
	if (FFileHelper::LoadFileToStringArray(partDefaultVals, *(ManifestDirectoryPath / TEXT("DefaultPartParams.txt"))))
	{
		FBIMPartSlotSpec::DefaultNamedParameterMap.Empty();
		for (auto& partValStr : partDefaultVals)
		{
			partValStr.RemoveSpacesInline();
			TArray<FString> partValPair;
			partValStr.ParseIntoArray(partValPair, TEXT("="));
			if (ensureAlways(partValPair.Num() == 2))
			{
				float inches = FCString::Atof(*partValPair[1].TrimStartAndEnd());
				FBIMPartSlotSpec::DefaultNamedParameterMap.Add(partValPair[0].TrimStartAndEnd(), FModumateUnitValue::WorldInches(inches));
			}
		}
	}
	const static FString BIMCacheFile = TEXT("_bimCache.json");
	FModumateBIMCacheRecord bimCacheRecord;
	if (!ReadBIMCache(BIMCacheFile, bimCacheRecord))
	{
		TArray<FString> errors;
		TArray<FGuid> starters;
		if (!ensureAlways(BIMPresetCollection.LoadCSVManifest(*ManifestDirectoryPath, BIMManifestFileName, starters, errors) == EBIMResult::Success))
		{
			return;
		}
		ensureAlways(errors.Num() == 0);
		bimCacheRecord.Presets = BIMPresetCollection;
		bimCacheRecord.Starters = starters;
		WriteBIMCache(BIMCacheFile, bimCacheRecord);
	}
	else
	{
		BIMPresetCollection = bimCacheRecord.Presets;
		BIMPresetCollection.PostLoad();
	}

	FString NCPString;
	FString NCPPath = ManifestDirectoryPath / TEXT("NCPTable.csv");
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

	typedef TFunction<void(const FBIMPresetInstance &Preset)> FAddAssetFunction;
	typedef TPair<FBIMTagPath, FAddAssetFunction> FAddAssetPath;
	TArray<FAddAssetPath> assetTargetPaths;

	FAddAssetFunction addMesh = [this](const FBIMPresetInstance& Preset)
	{
		FString assetPath = Preset.GetScopedProperty<FString>(EBIMValueScope::Mesh, BIMPropertyNames::AssetPath);

		if (ensureAlways(assetPath.Len() > 0))
		{
			FVector nativeSize = FVector(
				Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("NativeSizeX")),
				Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("NativeSizeY")),
				Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("NativeSizeZ"))
			) * UModumateDimensionStatics::CentimetersToInches;

			FBox nineSlice(
				FVector(Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("SliceX1")),
					Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("SliceY1")),
					Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("SliceZ1"))) * UModumateDimensionStatics::CentimetersToInches,

				FVector(Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("SliceX2")),
					Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("SliceY2")),
					Preset.GetScopedProperty<float>(EBIMValueScope::Mesh, TEXT("SliceZ2"))) * UModumateDimensionStatics::CentimetersToInches
			);

			FString name;
			Preset.TryGetProperty(BIMPropertyNames::Name, name);
			FString namedDimensions = Preset.GetScopedProperty<FString>(EBIMValueScope::Mesh,BIMPropertyNames::NamedDimensions);
			AddArchitecturalMesh(Preset.GUID, name, namedDimensions, nativeSize, nineSlice, assetPath);
		}
	};

	FAddAssetFunction addRawMaterial = [this](const FBIMPresetInstance& Preset)
	{
		FString assetPath;
		Preset.TryGetProperty(BIMPropertyNames::AssetPath,assetPath);
		if (assetPath.Len() != 0)
		{
			FString matName;
			if (ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Name, matName)))
			{
				AddArchitecturalMaterial(Preset.GUID, matName, assetPath);
			}
		}
	};

	FAddAssetFunction addMaterial = [this](const FBIMPresetInstance& Preset)
	{
		FGuid rawMaterial;

		for (auto& cp : Preset.ChildPresets)
		{
			const FBIMPresetInstance* childPreset = BIMPresetCollection.PresetFromGUID(cp.PresetGUID);
			if (childPreset != nullptr)
			{
				if (childPreset->NodeScope == EBIMValueScope::RawMaterial)
				{
					rawMaterial = cp.PresetGUID;
				}
			}
		}

		if (rawMaterial.IsValid())
		{
			const FBIMPresetInstance* preset = BIMPresetCollection.PresetFromGUID(rawMaterial);
			if (preset != nullptr)
			{
				FString assetPath, matName;
				if (ensureAlways(preset->TryGetProperty(BIMPropertyNames::AssetPath, assetPath) 
					&& Preset.TryGetProperty(BIMPropertyNames::Name, matName)))
				{
					AddArchitecturalMaterial(Preset.GUID, matName, assetPath);
				}
			}
		}
	};

	FAddAssetFunction addProfile = [this](const FBIMPresetInstance& Preset)
	{
		FString assetPath;
		Preset.TryGetProperty(BIMPropertyNames::AssetPath,assetPath);
		if (assetPath.Len() != 0)
		{
			FString name;
			if (ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Name, name)))
			{
				AddSimpleMesh(Preset.GUID, name, assetPath);
			}
		}
	};

	FAddAssetFunction addPattern = [this](const FBIMPresetInstance& Preset)
	{
		FLayerPattern newPattern;
		newPattern.InitFromCraftingPreset(Preset);
		Patterns.AddData(newPattern);

		FString assetPath;
		Preset.TryGetProperty(BIMPropertyNames::CraftingIconAssetFilePath, assetPath);
		if (assetPath.Len() > 0)
		{
			FString iconName;
			if (Preset.TryGetProperty(BIMPropertyNames::Name, iconName))
			{
				AddStaticIconTexture(Preset.GUID, iconName, assetPath);
			}
		}
	};

	/*
	Find the columns containing object type and asset destination (which db) for each preset type
	*/
	const FString objectTypeS = TEXT("ObjectType");
	int32 objectTypeI = -1;

	const FString assetTypeS = TEXT("AssetType");
	int32 assetTypeI = -1;

	const FString bimDesignerTitle = TEXT("BIMDesigner-NodeTitle");
	int32 bimDesignerTitleI = -1;

	for (int32 column = 0; column < NCPRows[0].Num(); ++column)
	{
		if (objectTypeS.Equals(NCPRows[0][column]))
		{
			objectTypeI = column;
		}
		else if (assetTypeS.Equals(NCPRows[0][column]))
		{
			assetTypeI = column;
		}
		else if (bimDesignerTitle.Equals(NCPRows[0][column]))
		{
			bimDesignerTitleI = column;
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
	const FString patternTag = TEXT("Pattern");

	typedef TFunction<void(const FBIMTagPath &TagPath)> FAssetTargetAssignment;
	TMap<FString, FAssetTargetAssignment> assetTargetMap;

	assetTargetMap.Add(meshTag, [addMesh, &assetTargetPaths](const FBIMTagPath& TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addMesh)); });
	assetTargetMap.Add(profileTag, [addProfile, &assetTargetPaths](const FBIMTagPath& TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addProfile)); });
	assetTargetMap.Add(rawMaterialTag, [addRawMaterial, &assetTargetPaths](const FBIMTagPath& TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addRawMaterial)); });
	assetTargetMap.Add(materialTag, [addMaterial, &assetTargetPaths](const FBIMTagPath& TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addMaterial)); });
	assetTargetMap.Add(patternTag, [addPattern, &assetTargetPaths](const FBIMTagPath& TagPath) {assetTargetPaths.Add(FAddAssetPath(TagPath, addPattern)); });

	typedef TPair<FBIMTagPath, EObjectType> FObjectPathRef;
	TArray<FObjectPathRef> objectPaths;

	typedef TPair<FBIMTagPath, FString> FTagTitleRef;
	TArray<FTagTitleRef> tagTitles;

	for (int32 row = 1; row < NCPRows.Num(); ++row)
	{
		FString tagStr = FString(NCPRows[row][0]).Replace(TEXT(" "), TEXT(""));
		if (tagStr.IsEmpty())
		{
			continue;
		}

		FBIMTagPath tagPath;
		tagPath.FromString(tagStr);

		// Get the object type cell and add it to the object map if applicable
		FString cell = NCPRows[row][objectTypeI];
		if (!cell.IsEmpty())
		{
			EObjectType ot = EnumValueByString(EObjectType, cell);
			objectPaths.Add(FObjectPathRef(tagPath, ot));
		}

		// Add asset loader if applicable
		cell = NCPRows[row][assetTypeI];
		if (!cell.IsEmpty())
		{
			FAssetTargetAssignment *assignment = assetTargetMap.Find(cell);
			if (assignment != nullptr)
			{
				(*assignment)(tagPath);
			}
		}

		cell = NCPRows[row][bimDesignerTitleI];
		if (!cell.IsEmpty())
		{
			tagTitles.Add(FTagTitleRef(tagPath, cell));
		}
	}

	// More specific matches further down the table
	Algo::Reverse(tagTitles);

	for (auto& preset : BIMPresetCollection.PresetsByGUID)
	{
		for (auto& tag : tagTitles)
		{
			if (preset.Value.MyTagPath.MatchesPartial(tag.Key))
			{
				preset.Value.CategoryTitle = FText::FromString(tag.Value);
			}
		}
	}

	const TArray<TPair<FString, EObjectType>> stubbies({
		TPair <FString,EObjectType>(TEXT("Part_FFE"),EObjectType::OTFurniture)
	});

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
	for (auto &kvp : BIMPresetCollection.PresetsByGUID)
	{
		if (kvp.Value.SlotConfigPresetGUID.IsValid())
		{
			const FBIMPresetInstance* slotConfig = BIMPresetCollection.PresetFromGUID(kvp.Value.SlotConfigPresetGUID);
			if (!ensureAlways(slotConfig != nullptr))
			{
				continue;
			}

			TArray<FBIMPresetPartSlot> EmptySlots;
			for (auto& configSlot : slotConfig->ChildPresets)
			{
				const FBIMPresetInstance* slotPreset = BIMPresetCollection.PresetFromGUID(configSlot.PresetGUID);
				if (!ensureAlways(slotPreset != nullptr))
				{
					continue;
				}

				FBIMPresetPartSlot* partSlot = kvp.Value.PartSlots.FindByPredicate([slotPreset](const FBIMPresetPartSlot& PartSlot)
				{
					if (PartSlot.SlotPresetGUID == slotPreset->GUID)
					{
						return true;
					}
					return false;
				});

				if (partSlot == nullptr)
				{
					FBIMPresetPartSlot& newSlot = EmptySlots.AddDefaulted_GetRef();
					newSlot.SlotPresetGUID = slotPreset->GUID;
				}
			}

			if (EmptySlots.Num() > 0)
			{
				kvp.Value.PartSlots.Append(EmptySlots);
			}
		}
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
			case EObjectType::OTFurniture:
				bimCacheRecord.Starters.Add(kvp.Value.GUID);
			break;
		}
	}

	/*
	Now build every preset that was returned in 'starters' by the manifest parse and add it to the project
	*/
	for (auto &starter : bimCacheRecord.Starters)
	{
		const FBIMPresetInstance* preset = BIMPresetCollection.PresetFromGUID(starter);

		// TODO: "starter" presets currently only refer to complete assemblies, will eventually include presets to be shopped from the marketplace
		if (ensureAlways(preset != nullptr) && preset->ObjectType == EObjectType::OTNone)
		{
			continue;
		}

		FBIMAssemblySpec outSpec;
		outSpec.FromPreset(*this, BIMPresetCollection, preset->GUID);
		outSpec.ObjectType = preset->ObjectType;
		BIMPresetCollection.UpdateProjectAssembly(outSpec);

		// TODO: default assemblies added to allow interim loading during assembly refactor, to be eliminated
		if (!BIMPresetCollection.DefaultAssembliesByObjectType.Contains(outSpec.ObjectType))
		{
			BIMPresetCollection.DefaultAssembliesByObjectType.Add(outSpec.ObjectType, outSpec);
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

const FArchitecturalMaterial *FModumateDatabase::GetArchitecturalMaterialByGUID(const FGuid& Key) const
{
	return AMaterials.GetData(Key);
}

const FArchitecturalMesh* FModumateDatabase::GetArchitecturalMeshByGUID(const FGuid& Key) const
{
	return AMeshes.GetData(Key);
}

const FSimpleMeshRef* FModumateDatabase::GetSimpleMeshByGUID(const FGuid& Key) const
{
	return SimpleMeshes.GetData(Key);
}

const FStaticIconTexture* FModumateDatabase::GetStaticIconTextureByGUID(const FGuid& Key) const
{
	return StaticIconTextures.GetData(Key);
}

const FLayerPattern* FModumateDatabase::GetPatternByGUID(const FGuid& GUID) const
{
	return Patterns.GetData(GUID);
}

bool FModumateDatabase::UnitTest()
{
	bool success = BIMPresetCollection.AssembliesByObjectType.Num() > 0;
	for (auto& kvdp : BIMPresetCollection.AssembliesByObjectType)
	{
		// Furniture is not crafted
		if (kvdp.Key == EObjectType::OTFurniture)
		{
			continue;
		};

		for (auto& kvp : kvdp.Value.DataMap)
		{
			FBIMPresetEditor editor;
			FBIMPresetEditorNodeSharedPtr root;
			success = ensureAlways(editor.InitFromPreset(BIMPresetCollection, kvp.Value.RootPreset, root) == EBIMResult::Success) && success;

			FBIMAssemblySpec editSpec;
			success = ensureAlways(editor.CreateAssemblyFromNodes(BIMPresetCollection, *this, editSpec) == EBIMResult::Success) && success;

			FBIMAssemblySpec makeSpec;
			success = ensureAlways(makeSpec.FromPreset(*this, BIMPresetCollection, editSpec.RootPreset) == EBIMResult::Success) && success;

			success = ensureAlways(FBIMAssemblySpec::StaticStruct()->CompareScriptStruct(&editSpec, &kvp.Value, PPF_None)) && success;
			success = ensureAlways(FBIMAssemblySpec::StaticStruct()->CompareScriptStruct(&makeSpec, &kvp.Value, PPF_None)) && success;
		}
	}
	return success;
}