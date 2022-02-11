// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"

#include "Objects/ModumateRoomStatics.h"

#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMPresetNCPTaxonomy.h"

#include "ModumateObjectDatabase.generated.h"

static constexpr int32 BIMCacheCurrentVersion = 19;
// Version 2: deprecate FBIMKeys for FGuids
// Version 3: move named parameters from meshes to presets
// Version 4: material binding editor
// Version 5: deprecate material bindings and use custom data instead
// Version 6: derive material channels from meshes
// Version 7: taxonomy in preset collection
// Version 8: strip NCP whitespace on CSV read
// Version 9: color tint variation as a percentage
// Version 10: patterns converted from child pins to properties
// Version 11: add upward blocker to taxonomy explorer
// Version 12: support for multiple custom data entries
// Version 13: support for miter priority in layer specs
// Version 14: miter priority struct rename
// Version 15: support for arbitrary enums in BIM forms
// Version 16: multiple assets added for 2.0.0.0
// Version 17: point-hosted objects
// Version 18: user-editable part sizes
// Version 19: face-hosted objects

USTRUCT()
struct FModumateBIMCacheRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = BIMCacheCurrentVersion;

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

	TModumateDataCollection<FRoomConfiguration> RoomConfigurations;
};
