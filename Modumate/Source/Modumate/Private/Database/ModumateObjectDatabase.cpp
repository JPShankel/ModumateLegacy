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
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "HAL/FileManagerGeneric.h"

TAutoConsoleVariable<bool> CVarModumateUseAllPresets(
	TEXT("modumate.UseAllPresets"),
	false,
	TEXT("Whether to start new projects with all available presets."),
	ECVF_Default);



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
	// Make sure abstract material is in for default use
	FGuid::Parse(TEXT("09F17296-2023-944C-A1E7-EEDFE28680E9"), DefaultMaterialGUID);

	TArray<FGuid> starters;
	TArray<FString> errors;

	if (!ensure(BIMPresetCollection.LoadCSVManifest(*ManifestDirectoryPath, BIMManifestFileName, starters, errors) == EBIMResult::Success))
	{
		return;
	}
	
	if (!ensure(errors.Num() == 0))
	{
		return;
	}

	FString NCPString;
	FString NCPPath = FPaths::Combine(*ManifestDirectoryPath, BIMNCPFileName);
	if (!ensure(FFileHelper::LoadFileToString(NCPString, *NCPPath)))
	{
		return;
	}

	FCsvParser NCPParsed(NCPString);
	const FCsvParser::FRows& NCPRows = NCPParsed.GetRows();

	if (ensure(BIMPresetCollection.PresetTaxonomy.LoadCSVRows(NCPRows) == EBIMResult::Success))
	{
		ensure(BIMPresetCollection.PostLoad() == EBIMResult::Success);
	}

	// If this preset implies an asset type, load it
	for (auto& preset : BIMPresetCollection.PresetsByGUID)
	{
		FLightConfiguration lightConfig;
		if (preset.Value.TryGetCustomData(lightConfig))
		{
			const FBIMPresetInstance* bimPresetInstance = BIMPresetCollection.PresetsByGUID.Find(lightConfig.PresetGUID);
			if (bimPresetInstance != nullptr)
			{
				FString assetPath = bimPresetInstance->GetScopedProperty<FString>(EBIMValueScope::IESProfile, BIMPropertyNames::AssetPath);
				FSoftObjectPath referencePath = FString(TEXT("TextureLightProfile'")).Append(assetPath).Append(TEXT("'"));
				TSharedPtr<FStreamableHandle> SyncStreamableHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(referencePath);
				if (SyncStreamableHandle)
				{
					UTextureLightProfile* IESProfile = Cast<UTextureLightProfile>(SyncStreamableHandle->GetLoadedAsset());
					if (IESProfile)
					{
						lightConfig.LightProfile = IESProfile;
					}
				}
				preset.Value.SetCustomData(lightConfig);
			}
		}
	}

	BIMPresetCollection.ForEachPreset([this](const FBIMPresetInstance& Preset) {
		AddAssetsFromPreset(Preset);
		});
	BIMPresetCollection.SetPartSizesFromMeshes();

	BIMUntruncatedCollection = BIMPresetCollection;

	TSet<FGuid> presetsToAdd;
	for (auto& starter : starters)
	{
		FBIMPresetInstance* preset = BIMUntruncatedCollection.PresetFromGUID(starter);
		if (ensure(preset != nullptr))
		{
			presetsToAdd.Add(preset->GUID);
			TArray<FGuid> descendents;
			BIMUntruncatedCollection.GetAllDescendentPresets(starter, descendents);
			for (auto& descendent : descendents)
			{
				presetsToAdd.Add(descendent);
			}
		}
	}

	if (!CVarModumateUseAllPresets.GetValueOnAnyThread())
	{
		BIMPresetCollection.PresetsByGUID.Empty();

		for (auto& guid : presetsToAdd)
		{
			FBIMPresetInstance* preset = BIMUntruncatedCollection.PresetFromGUID(guid);
			if (ensure(preset != nullptr))
			{
				BIMPresetCollection.AddPreset(*preset);
			}
		}
	}
	else
	{
		for (auto& kvp : BIMPresetCollection.PresetsByGUID)
		{
			if (kvp.Value.ObjectType != EObjectType::OTNone)
			{
				starters.Add(kvp.Value.GUID);
			}
		}
	}

	ensure(BIMPresetCollection.ProcessStarterAssemblies(*this, starters) == EBIMResult::Success);
}

bool FModumateDatabase::AddAssetsFromPreset(const FBIMPresetInstance& Preset)
{
	switch (Preset.AssetType)
	{
	case EBIMAssetType::IESProfile:
		return AddLightFromPreset(Preset);
	case EBIMAssetType::Mesh:
		return AddMeshFromPreset(Preset);
	case EBIMAssetType::Profile:
		return AddProfileFromPreset(Preset);
	case EBIMAssetType::RawMaterial:
		return AddRawMaterialFromPreset(Preset);
	case EBIMAssetType::Material:
		return AddMaterialFromPreset(Preset);
	case EBIMAssetType::Pattern:
		return AddPatternFromPreset(Preset);
	};

	return false;
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
		if (assetPath.StartsWith(TEXT("http")))
		{
			AddArchitecturalMeshFromDatasmith(assetPath, Preset.GUID);
		}
		else
		{
			AddArchitecturalMesh(Preset.GUID, name, meshNativeSize, meshNineSlice, assetPath);
		}
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
	//fetch IES resource from profile path
	//fill in light configuration
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
	bool bSuccess = BIMPresetCollection.AssembliesByObjectType.Num() > 0;
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
			bSuccess = ensureAlways(editor.InitFromPreset(*this, kvp.Value.PresetGUID, root) == EBIMResult::Success) && bSuccess;

			FBIMAssemblySpec editSpec;
			bSuccess = ensureAlways(editor.CreateAssemblyFromNodes(*this, editSpec) == EBIMResult::Success) && bSuccess;

			FBIMAssemblySpec makeSpec;
			if (!ensureAlways(makeSpec.FromPreset(*this, FBIMPresetCollectionProxy(BIMPresetCollection), editSpec.PresetGUID) == EBIMResult::Success))
			{
				bSuccess = false;
				makeSpec.FromPreset(*this, FBIMPresetCollectionProxy(BIMPresetCollection), editSpec.PresetGUID);
			}

			UScriptStruct* scriptStruct = FBIMAssemblySpec::StaticStruct();

			bSuccess = ensureAlways(FBIMAssemblySpec::StaticStruct()->CompareScriptStruct(&editSpec, &kvp.Value, PPF_None)) && bSuccess;
			bSuccess = ensureAlways(FBIMAssemblySpec::StaticStruct()->CompareScriptStruct(&makeSpec, &kvp.Value, PPF_None)) && bSuccess;
		}
	}

	FBIMPresetInstance edgePreset;
	BIMPresetCollection.GetBlankPresetForObjectType(EObjectType::OTEdgeDetail, edgePreset);
	bSuccess = !edgePreset.GUID.IsValid() && bSuccess;

	auto createEdgeDelta = BIMPresetCollection.MakeCreateNewDelta(edgePreset);
	bSuccess = !createEdgeDelta->OldState.GUID.IsValid() && bSuccess;
	bSuccess = createEdgeDelta->NewState.GUID.IsValid() && bSuccess;

	// Note: ApplyDelta is in Document...simulate effect here
	bSuccess = (BIMPresetCollection.AddPreset(createEdgeDelta->NewState) == EBIMResult::Success) && bSuccess;
	const FBIMPresetInstance* edgeInst = BIMPresetCollection.PresetFromGUID(createEdgeDelta->NewState.GUID);
	bSuccess = (edgeInst != nullptr) && bSuccess;

	auto updateEdgeDelta = BIMPresetCollection.MakeUpdateDelta(createEdgeDelta->NewState);
	bSuccess = (updateEdgeDelta->OldState.GUID == updateEdgeDelta->NewState.GUID) && bSuccess;
	bSuccess = updateEdgeDelta->NewState.GUID.IsValid() && bSuccess;

	return bSuccess;
}

void FModumateDatabase::AddArchitecturalMeshFromDatasmith(const FString& AssetUrl, const FGuid& InArchitecturalMeshKey)
{
	FArchitecturalMesh mesh;
	//mesh.NativeSize = InNativeSize;
	//mesh.NineSliceBox = InNineSliceBox;
	mesh.Key = InArchitecturalMeshKey;
	mesh.DatasmithUrl = AssetUrl;

	AMeshes.AddData(mesh);
}
