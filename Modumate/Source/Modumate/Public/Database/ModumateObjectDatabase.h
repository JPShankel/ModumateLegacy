// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataCollection.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "ModumateCore/ModumateRoomStatics.h"

/**
 *
 */


class MODUMATE_API FModumateDatabase
{

private:

	TModumateDataCollection<FArchitecturalMaterial> AMaterials;
	TModumateDataCollection<FArchitecturalMesh> AMeshes;
	TModumateDataCollection<FCustomColor> NamedColors;
	TModumateDataCollection<FSimpleMeshRef> SimpleMeshes;
	TModumateDataCollection<FStaticIconTexture> StaticIconTextures;

	void AddArchitecturalMaterial(const FName& Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddArchitecturalMesh(const FName& Key, const FString& Name, const FVector& InNativeSize, const FBox& InNineSliceBox, const FSoftObjectPath& AssetPath);
	void AddSimpleMesh(const FName& Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddCustomColor(const FName& Key, const FString& Name, const FString& HexValue);

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
	const FArchitecturalMesh* GetArchitecturalMeshByKey(const FName& Key) const;
	const FArchitecturalMaterial *GetArchitecturalMaterialByKey(const FName& Key) const;
	const FCustomColor *GetCustomColorByKey(const FName &Key) const;
	const FSimpleMeshRef *GetSimpleMeshByKey(const FName &Key) const;
	const Modumate::FRoomConfiguration *GetRoomConfigByKey(const FName &Key) const;
	const FStaticIconTexture *GetStaticIconTextureByKey(const FName &Key) const;

	TArray<FString> GetDebugInfo();

	TModumateDataCollection<Modumate::FRoomConfiguration> RoomConfigurations;
};
