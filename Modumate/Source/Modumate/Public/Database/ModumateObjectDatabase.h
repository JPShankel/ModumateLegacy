// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"

#include "ModumateCore/ModumateRoomStatics.h"

#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMPresetNCPTaxonomy.h"

#include "ModumateObjectDatabase.generated.h"

static constexpr int32 BIMCacheCurrentVersion = 7;
// Version 2: deprecate FBIMKeys for FGuids
// Version 3: move named parameters from meshes to presets
// Version 4: material binding editor
// Version 5: deprecate material bindings and use custom data instead
// Version 6: derive material channels from meshes
// Version 7: taxonomy in preset collection

USTRUCT()
struct FModumateBIMCacheRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version;

	UPROPERTY()
	FBIMPresetCollection Presets;

	UPROPERTY()
	TArray<FGuid> Starters;

	UPROPERTY()
	FBIMPresetNCPTaxonomy PresetTaxonomy;
};

class MODUMATE_API FModumateDatabase
{
private:

	TModumateDataCollection<FArchitecturalMaterial> AMaterials;
	TModumateDataCollection<FArchitecturalMesh> AMeshes;
	TModumateDataCollection<FSimpleMeshRef> SimpleMeshes;
	TModumateDataCollection<FStaticIconTexture> StaticIconTextures;
	TModumateDataCollection<FLayerPattern> Patterns;

	void AddArchitecturalMaterial(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddArchitecturalMesh(const FGuid& Key, const FString& Name, const FVector& InNativeSize, const FBox& InNineSliceBox, const FSoftObjectPath& AssetPath);
	void AddSimpleMesh(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddStaticIconTexture(const FGuid& Key, const FString& Name, const FSoftObjectPath& AssetPath);

	FBIMPresetCollection BIMPresetCollection;

	FString ManifestDirectoryPath;

	FGuid DefaultMaterialGUID;

	bool AddMeshFromPreset(const FBIMPresetInstance& Preset);
	bool AddRawMaterialFromPreset(const FBIMPresetInstance& Preset);
	bool AddMaterialFromPreset(const FBIMPresetInstance& Preset);
	bool AddProfileFromPreset(const FBIMPresetInstance& Preset);
	bool AddPatternFromPreset(const FBIMPresetInstance& Preset);

public:

	FModumateDatabase();
	~FModumateDatabase();

	void Init();
	void Shutdown();

	// Read database
	void ReadPresetData();

	const FBIMPresetCollection& GetPresetCollection() const { return BIMPresetCollection; }

	// Data Access
	const FArchitecturalMesh* GetArchitecturalMeshByGUID(const FGuid& Key) const;
	const FArchitecturalMaterial* GetArchitecturalMaterialByGUID(const FGuid& Key) const;
	const FSimpleMeshRef* GetSimpleMeshByGUID(const FGuid& Key) const;
	const FStaticIconTexture* GetStaticIconTextureByGUID(const FGuid& Key) const;
	const FLayerPattern* GetPatternByGUID(const FGuid& Key) const;

	FGuid GetDefaultMaterialGUID() const;

	bool ReadBIMCache(const FString& CacheFile, FModumateBIMCacheRecord& OutCache);
	bool WriteBIMCache(const FString& CacheFile, const FModumateBIMCacheRecord& InCache) const;

	bool UnitTest();

	TModumateDataCollection<Modumate::FRoomConfiguration> RoomConfigurations;
};
