// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"

#include "DocumentManagement/ModumatePresetManager.h"
#include "ModumateCore/ModumateRoomStatics.h"

#include "BIMKernel/BIMKey.h"
#include "BIMKernel/BIMLegacyPattern.h"

class MODUMATE_API FModumateDatabase
{
private:

	TModumateDataCollection<FArchitecturalMaterial> AMaterials;
	TModumateDataCollection<FArchitecturalMesh> AMeshes;
	TModumateDataCollection<FCustomColor> NamedColors;
	TModumateDataCollection<FSimpleMeshRef> SimpleMeshes;
	TModumateDataCollection<FStaticIconTexture> StaticIconTextures;
	TModumateDataCollection<FLayerPattern> Patterns;

	void AddArchitecturalMaterial(const FBIMKey& Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddArchitecturalMesh(const FBIMKey& Key, const FString& Name, const FString& InNamedParameters, const FVector& InNativeSize, const FBox& InNineSliceBox, const FSoftObjectPath& AssetPath);
	void AddSimpleMesh(const FBIMKey& Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddCustomColor(const FBIMKey& Key, const FString& Name, const FString& HexValue);

	FPresetManager PresetManager;

public:

	FModumateDatabase();
	~FModumateDatabase();

	void Init();
	void Shutdown();

	// Read database
	void ReadRoomConfigurations(UDataTable* data);
	void ReadPresetData();

	void InitPresetManagerForNewDocument(FPresetManager &OutManager) const;

	// Data Access
	const FArchitecturalMesh* GetArchitecturalMeshByKey(const FBIMKey& Key) const;
	const FArchitecturalMaterial* GetArchitecturalMaterialByKey(const FBIMKey& Key) const;
	const FCustomColor* GetCustomColorByKey(const FBIMKey& Key) const;
	const FSimpleMeshRef* GetSimpleMeshByKey(const FBIMKey& Key) const;
	const Modumate::FRoomConfiguration* GetRoomConfigByKey(const FBIMKey& Key) const;
	const FStaticIconTexture* GetStaticIconTextureByKey(const FBIMKey& Key) const;
	const FLayerPattern* GetLayerByKey(const FBIMKey& Key) const;

	TArray<FString> GetDebugInfo();

	TModumateDataCollection<Modumate::FRoomConfiguration> RoomConfigurations;
};
