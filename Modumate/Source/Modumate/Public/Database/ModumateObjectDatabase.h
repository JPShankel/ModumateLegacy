// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "BIMKernel/BIMLegacyPattern.h"
#include "Database/ModumateDataCollection.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateTypes.h"
#include "BIMKernel/BIMProperties.h"
#include "ModumateDataTables.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "ModumateCore/ModumateRoomStatics.h"

/**
 *
 */


class MODUMATE_API FModumateDatabase
{
private:

	// Primitive type
	TModumateDataCollection<FArchitecturalMaterial> AMaterials;
	TModumateDataCollection<FArchitecturalMesh> AMeshes;
	TModumateDataCollection<FCustomColor> NamedColors;
	TModumateDataCollection<FLightConfiguration> LightConfigs;
	TModumateDataCollection<FSimpleMeshRef> SimpleMeshes;

public:

	TModumateDataCollection<Modumate::FRoomConfiguration> RoomConfigurations;
	TModumateDataCollection<FStaticIconTexture> StaticIconTextures;

	FPresetManager PresetManager;
	FModumateDatabase();
	~FModumateDatabase();

	void Init();
	void Shutdown();

	// Read database
	void ReadLightConfigData(UDataTable *data);
	void ReadRoomConfigurations(UDataTable *data);

	void AddArchitecturalMaterial(const FName &Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddArchitecturalMesh(const FName &Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddSimpleMesh(const FName& Key, const FString& Name, const FSoftObjectPath& AssetPath);
	void AddCustomColor(const FName& Key, const FString& Name, const FString &HexValue);

	void ReadPresetData();
	void InitPresetManagerForNewDocument(FPresetManager &OutManager) const;

	// Data Access
	const FArchitecturalMesh* GetArchitecturalMeshByKey(const FName& Key) const;
	const FArchitecturalMaterial *GetArchitecturalMaterialByKey(const FName& Key) const;
	const FCustomColor *GetCustomColorByKey(const FName &Key) const;
	const FSimpleMeshRef *GetSimpleMeshByKey(const FName &Key) const;
	const Modumate::FRoomConfiguration *GetRoomConfigByKey(const FName &Key) const;
	const FStaticIconTexture *GetStaticIconTextureByKey(const FName &Key) const;

	bool ParseColorFromField(FCustomColor &OutColor, const FString &Field);

	TArray<FString> GetDebugInfo();
};
