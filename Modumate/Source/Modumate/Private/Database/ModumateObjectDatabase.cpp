// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateObjectDatabase.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/EdgeDetailData.h"
#include "Database/ModumateArchitecturalLight.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
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

void FModumateDatabase::AddArchitecturalMesh(const FGuid& Key, const FString& Name, const FVector& InNativeSize, const FBox& InNineSliceBox, const FSoftObjectPath& AssetPath)
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
static constexpr TCHAR BIMCacheRecordField[] = TEXT("BIMCacheRecord");
static constexpr TCHAR BIMManifestFileName[] = TEXT("BIMManifest.txt");
static constexpr TCHAR BIMNCPFileName[] = TEXT("NCPTable.csv");

bool FModumateDatabase::ReadBIMCache(const FString& CacheFile, FModumateBIMCacheRecord& OutCache)
{
	// Worker instances need to support multiple versions of the BIM data, so no caching
#if UE_SERVER
	return false;
#endif

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

	FString ncpFilePath = FPaths::Combine(*ManifestDirectoryPath, BIMNCPFileName);
	FDateTime ncpFileDate = IFileManager::Get().GetTimeStamp(*ncpFilePath);
	if (ncpFileDate > cacheDate)
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

	auto cacheObRef = cacheOb->ToSharedRef();

	// Only the current version of BIM cache is supported; both older and newer versions may ungracefully fail during JSON deserialization.
	// Grab the version from the JSON before full deserialization, to prevent potential future errors.
	const static FString versionFieldName(TEXT("Version"));
	int32 cacheVersion;
	if (!cacheObRef->TryGetNumberField(versionFieldName, cacheVersion) ||
		(cacheVersion != BIMCacheCurrentVersion))
	{
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectToUStruct<FModumateBIMCacheRecord>(cacheOb->ToSharedRef(), &OutCache))
	{
		return false;
	}

	return true;
}

bool FModumateDatabase::WriteBIMCache(const FString& CacheFile, const FModumateBIMCacheRecord& InCache) const
{
	FString cacheFile = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), CacheFile);
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*cacheFile);

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

FGuid FModumateDatabase::GetDefaultMaterialGUID() const
{
	return DefaultMaterialGUID;
}

/*
This function is in development pending a complete data import/access plan
In the meantime, we read a manifest of CSV files and look for expected presets to populate tools
*/
void FModumateDatabase::ReadPresetData()
{
	const static FString BIMCacheFile = TEXT("_bimCache.json");
	FModumateBIMCacheRecord bimCacheRecord;

#if !UE_BUILD_SHIPPING
	bool bWantUnitTest = false;
#endif

	if (!ReadBIMCache(BIMCacheFile, bimCacheRecord))
	{
		TArray<FString> errors;
		TArray<FGuid> starters;
		if (!ensureAlways(BIMPresetCollection.LoadCSVManifest(*ManifestDirectoryPath, BIMManifestFileName, starters, errors) == EBIMResult::Success))
		{
			return;
		}
		if (ensureAlways(errors.Num() == 0))
		{
			FString NCPString;
			FString NCPPath = FPaths::Combine(*ManifestDirectoryPath, BIMNCPFileName);
			if (!ensureAlways(FFileHelper::LoadFileToString(NCPString, *NCPPath)))
			{
				return;
			}

			FCsvParser NCPParsed(NCPString);
			const FCsvParser::FRows& NCPRows = NCPParsed.GetRows();

			if (ensureAlways(BIMPresetCollection.PresetTaxonomy.LoadCSVRows(NCPRows) == EBIMResult::Success))
			{
				bimCacheRecord = FModumateBIMCacheRecord();
				bimCacheRecord.Version = BIMCacheCurrentVersion;
				bimCacheRecord.Presets = BIMPresetCollection;

				ensureAlways(BIMPresetCollection.PostLoad() == EBIMResult::Success);

				TArray<FGuid> furniture;
				BIMPresetCollection.GetPresetsByPredicate(
					[](const FBIMPresetInstance& Preset) {
						return Preset.ObjectType == EObjectType::OTFurniture ||
							Preset.ObjectType == EObjectType::OTPointHosted ||
							Preset.ObjectType == EObjectType::OTEdgeHosted ||
							Preset.ObjectType == EObjectType::OTFaceHosted; },
					furniture);

				bimCacheRecord.Starters = starters;
				bimCacheRecord.Starters.Append(furniture);
				bimCacheRecord.PresetTaxonomy = BIMPresetCollection.PresetTaxonomy;

#if !UE_SERVER
				WriteBIMCache(BIMCacheFile, bimCacheRecord);
#endif
			}
#if !UE_BUILD_SHIPPING
			bWantUnitTest = true;
#endif
		}
	}
	else
	{
		BIMPresetCollection = bimCacheRecord.Presets;
		BIMPresetCollection.PresetTaxonomy = bimCacheRecord.PresetTaxonomy;
		ensureAlways(BIMPresetCollection.PostLoad() == EBIMResult::Success);
	}

	// If this preset implies an asset type, load it
	for (auto& preset : BIMPresetCollection.PresetsByGUID)
	{
		switch (preset.Value.AssetType)
		{
			case EBIMAssetType::IESProfile:
				AddLightFromPreset(preset.Value);
				break;
			case EBIMAssetType::Mesh:
				AddMeshFromPreset(preset.Value);
				break;
			case EBIMAssetType::Profile:
				AddProfileFromPreset(preset.Value);
				break;
			case EBIMAssetType::RawMaterial:
				AddRawMaterialFromPreset(preset.Value);
				break;
			case EBIMAssetType::Material:
				AddMaterialFromPreset(preset.Value);
				break;
			case EBIMAssetType::Pattern:
				AddPatternFromPreset(preset.Value);
				break;
		};
	}

	// When swapping a mesh, a preset may inherit material mappings it does not yet define, so we need a default material
	FGuid abstractMaterialGuid;
	FGuid::Parse(TEXT("09F17296-2023-944C-A1E7-EEDFE28680E9"), DefaultMaterialGUID);
	const FArchitecturalMaterial* abstractMaterial = GetArchitecturalMaterialByGUID(DefaultMaterialGUID);
	ensureAlways(abstractMaterial != nullptr);

	BIMPresetCollection.SetPartSizesFromMeshes();

	ensureAlways(BIMPresetCollection.ProcessStarterAssemblies(*this, bimCacheRecord.Starters) == EBIMResult::Success);

#if !UE_BUILD_SHIPPING
	if (bWantUnitTest)
	{
		ensureAlways(UnitTest());
	}
#endif
}

bool FModumateDatabase::AddMeshFromPreset(const FBIMPresetInstance& Preset)
{
	FString assetPath = Preset.GetScopedProperty<FString>(EBIMValueScope::Mesh, BIMPropertyNames::AssetPath);

	if (ensureAlways(assetPath.Len() > 0))
	{
		FVector meshNativeSize(ForceInitToZero);
		FBox meshNineSlice(ForceInitToZero);

		FVector presetNativeSize(ForceInitToZero);
		if (Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("NativeSizeX"), presetNativeSize.X) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("NativeSizeY"), presetNativeSize.Y) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("NativeSizeZ"), presetNativeSize.Z))
		{
			meshNativeSize = presetNativeSize * UModumateDimensionStatics::CentimetersToInches;
		}

		FVector presetNineSliceMin(ForceInitToZero), presetNineSliceMax(ForceInitToZero);
		if (Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceX1"), presetNineSliceMin.X) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceY1"), presetNineSliceMin.Y) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceZ1"), presetNineSliceMin.Z) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceX2"), presetNineSliceMax.X) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceY2"), presetNineSliceMax.Y) &&
			Preset.Properties.TryGetProperty(EBIMValueScope::Mesh, TEXT("SliceZ2"), presetNineSliceMax.Z))
		{
			meshNineSlice = FBox(
				presetNineSliceMin * UModumateDimensionStatics::CentimetersToInches,
				presetNineSliceMax * UModumateDimensionStatics::CentimetersToInches);
		}

		FString name;
		Preset.TryGetProperty(BIMPropertyNames::Name, name);
		AddArchitecturalMesh(Preset.GUID, name, meshNativeSize, meshNineSlice, assetPath);
	}
	return true;
}

bool FModumateDatabase::AddRawMaterialFromPreset(const FBIMPresetInstance& Preset)
{
	FString assetPath;
	Preset.TryGetProperty(BIMPropertyNames::AssetPath, assetPath);
	if (assetPath.Len() != 0)
	{
		FString matName;
		if (ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Name, matName)))
		{
			AddArchitecturalMaterial(Preset.GUID, matName, assetPath);
			return true;
		}
	}
	return false;
}

bool FModumateDatabase::AddMaterialFromPreset(const FBIMPresetInstance& Preset)
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
				return true;
			}
		}
	}
	return false;
}

bool FModumateDatabase::AddLightFromPreset(const FBIMPresetInstance& Preset)
{
	FArchitecturalLight light;
	light.Key = Preset.GUID;

	Preset.TryGetProperty(BIMPropertyNames::Name, light.DisplayName);
	Preset.TryGetProperty(BIMPropertyNames::CraftingIconAssetFilePath, light.IconPath);
	Preset.TryGetProperty(BIMPropertyNames::AssetPath, light.ProfilePath);

	Lights.AddData(light);

	return true;
}

bool FModumateDatabase::AddProfileFromPreset(const FBIMPresetInstance& Preset)
{
	FString assetPath;
	Preset.TryGetProperty(BIMPropertyNames::AssetPath, assetPath);
	if (assetPath.Len() != 0)
	{
		FString name;
		if (ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Name, name)))
		{
			AddSimpleMesh(Preset.GUID, name, assetPath);
			return true;
		}
	}
	return false;
}

bool FModumateDatabase::AddPatternFromPreset(const FBIMPresetInstance& Preset)
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
			return true;
		}
	}
	return false;
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
	FBIMPresetCollectionProxy presetCollection(BIMPresetCollection);
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
			editor.PresetCollectionProxy = presetCollection;
			success = ensureAlways(editor.InitFromPreset(*this, kvp.Value.PresetGUID, root) == EBIMResult::Success) && success;

			FBIMAssemblySpec editSpec;
			success = ensureAlways(editor.CreateAssemblyFromNodes(*this, editSpec) == EBIMResult::Success) && success;

			FBIMAssemblySpec makeSpec;
			success = ensureAlways(makeSpec.FromPreset(*this, FBIMPresetCollectionProxy(BIMPresetCollection), editSpec.PresetGUID) == EBIMResult::Success) && success;

			success = ensureAlways(FBIMAssemblySpec::StaticStruct()->CompareScriptStruct(&editSpec, &kvp.Value, PPF_None)) && success;
			success = ensureAlways(FBIMAssemblySpec::StaticStruct()->CompareScriptStruct(&makeSpec, &kvp.Value, PPF_None)) && success;
		}
	}

	FBIMPresetInstance edgePreset;
	BIMPresetCollection.GetBlankPresetForObjectType(EObjectType::OTEdgeDetail, edgePreset);
	success = !edgePreset.GUID.IsValid() && success;

	auto createEdgeDelta = BIMPresetCollection.MakeCreateNewDelta(edgePreset);
	success = !createEdgeDelta->OldState.GUID.IsValid() && success;
	success = createEdgeDelta->NewState.GUID.IsValid() && success;

	// Note: ApplyDelta is in Document...simulate effect here
	success = (BIMPresetCollection.AddPreset(createEdgeDelta->NewState) == EBIMResult::Success) && success;
	const FBIMPresetInstance* edgeInst = BIMPresetCollection.PresetFromGUID(createEdgeDelta->NewState.GUID);
	success = (edgeInst != nullptr) && success;

	auto updateEdgeDelta = BIMPresetCollection.MakeUpdateDelta(createEdgeDelta->NewState);
	success = (updateEdgeDelta->OldState.GUID == updateEdgeDelta->NewState.GUID) && success;
	success = updateEdgeDelta->NewState.GUID.IsValid() && success;

	return success;
}
