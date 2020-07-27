// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectAssembly.h"
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
	TModumateDataCollection<FModumateObjectAssemblyLayer> FFEParts;
	TModumateDataCollection<FArchitecturalMaterial> AMaterials;
	TModumateDataCollection<FArchitecturalMesh> AMeshes;
	TModumateDataCollection<FCustomColor> NamedColors;
	TModumateDataCollection<FLightConfiguration> LightConfigs;
	TModumateDataCollection<FSimpleMeshRef> SimpleMeshes;

public:

	TModumateDataCollection<FModumateObjectAssembly> FFEAssemblies;
	TModumateDataCollection<FPortalPart> PortalParts;
	TModumateDataCollection<Modumate::FRoomConfiguration> RoomConfigurations;
	TModumateDataCollection<FStaticIconTexture> StaticIconTextures;

	// Option sets
	TModumateDataCollection<Modumate::FCraftingOptionSet> PatternOptionSets;
	TModumateDataCollection<Modumate::FCraftingOptionSet> ProfileOptionSets;

	TModumateDataCollection<Modumate::FPortalAssemblyConfigurationOptionSet> PortalConfigurationOptionSets;
	TModumateDataCollection<Modumate::FCraftingPortalPartOptionSet> PortalPartOptionSets;

	FModumateDatabase();
	~FModumateDatabase();

	void Init();
	void Shutdown();

	// Read database
	void ReadMeshData(UDataTable *data);
	void ReadFFEPartData(UDataTable *data);
	void ReadLightConfigData(UDataTable *data);
	void ReadFFEAssemblyData(UDataTable *data);
	void ReadColorData(UDataTable *data);
	void ReadPortalPartData(UDataTable *data);
	void ReadRoomConfigurations(UDataTable *data);
	void ReadPortalConfigurationData(UDataTable *data);
	void ReadCraftingPatternOptionSet(UDataTable *data);
	void ReadCraftingPortalPartOptionSet(UDataTable *data);
	void ReadCraftingProfileOptionSet(UDataTable *data);

	void AddArchitecturalMaterial(const FName Key, const FString &Name, const FSoftObjectPath &AssetPath);

	void ReadPresetData();
	void InitPresetManagerForNewDocument(FPresetManager &OutManager) const;
	FPresetManager PresetManager;
	void ReadMarketplace(UWorld *world);

	// Data Access
	const FArchitecturalMaterial *GetArchitecturalMaterialByKey(const FName &name) const;
	const FCustomColor *GetCustomColorByKey(const FName &Key) const;
	const FPortalPart *GetPortalPartByKey(const FName &Key) const;
	const FSimpleMeshRef *GetSimpleMeshByKey(const FName &Key) const;
	const Modumate::FRoomConfiguration *GetRoomConfigByKey(const FName &Key) const;
	const FStaticIconTexture *GetStaticIconTextureByKey(const FName &Key) const;

	bool ParseColorFromField(FCustomColor &OutColor, const FString &Field);
	bool ParsePortalConfigDimensionSets(FName configKey, const FString &typeString, const TArray<FString> &setStrings, TArray<Modumate::FPortalConfigDimensionSet> &outSets);

	TArray<FString> GetDebugInfo();
};
