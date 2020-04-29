// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <functional>
#include "ModumateObjectAssembly.h"
#include "ModumateLayerPattern.h"
#include "ModumateDecisionTree.h"
#include "ModumateDataCollection.h"
#include "ModumateSerialization.h"
#include "ModumateTypes.h"
#include "ModumateBIMSchema.h"
#include "ModumateDataTables.h"
#include "ModumatePresetManager.h"
#include "ModumateRoomStatics.h"

/**
 *
 */


namespace Modumate
{
	class MODUMATE_API ModumateObjectDatabase
	{
	private:

		// Primitive type
		DataCollection<FModumateObjectAssemblyLayer> FFEParts;
		DataCollection<FArchitecturalMaterial> AMaterials;
		DataCollection<FArchitecturalMesh> AMeshes;
		DataCollection<FCustomColor> NamedColors;
		DataCollection<FLightConfiguration> LightConfigs;
		DataCollection<FSimpleMeshRef> SimpleMeshes;

	public:

		DataCollection<FCraftingSubcategoryData> CraftingSubcategoryData;
		DataCollection<FModumateObjectAssembly> FFEAssemblies;
		DataCollection<FPortalPart> PortalParts;
		DataCollection<FRoomConfiguration> RoomConfigurations;
		DataCollection<FStaticIconTexture> StaticIconTextures;


		// Option sets
		DataCollection<FCraftingOptionSet> LayerMaterialColors, ModuleMaterialColors, GapMaterialColors, FinishMaterialColors;
		DataCollection<FCraftingOptionSet> LayerModuleOptionSets, LayerGapOptionSets, ToeKickOptionSets;
		DataCollection<FCraftingOptionSet> LayerThicknessOptionSets;
		DataCollection<FCraftingOptionSet> PatternOptionSets;
		DataCollection<FCraftingOptionSet> ProfileOptionSets;

		DataCollection<FPortalAssemblyConfigurationOptionSet> PortalConfigurationOptionSets;
		DataCollection<FCraftingPortalPartOptionSet> PortalPartOptionSets;

		ModumateObjectDatabase();
		~ModumateObjectDatabase();

		void Init();
		void Shutdown();

		// Read database
		void ReadAMaterialData(UDataTable *data);
		void ReadMeshData(UDataTable *data);
		void ReadFFEPartData(UDataTable *data);
		void ReadLightConfigData(UDataTable *data);
		void ReadFFEAssemblyData(UDataTable *data);
		void ReadColorData(UDataTable *data);
		void ReadPortalPartData(UDataTable *data);
		void ReadRoomConfigurations(UDataTable *data);
		void ReadPortalConfigurationData(UDataTable *data);
		void ReadCraftingSubcategoryData(UDataTable *data);
		void ReadCraftingLayerThicknessOptionSet(UDataTable *data);
		void ReadCraftingDimensionalOptionSet(UDataTable *data);
		void ReadCraftingMaterialAndColorOptionSet(UDataTable *data);
		void ReadCraftingPatternOptionSet(UDataTable *data);
		void ReadCraftingPortalPartOptionSet(UDataTable *data);
		void ReadCraftingProfileOptionSet(UDataTable *data);

		void ReadMarketplace(UWorld *world);
		void InitPresetManagerForNewDocument(FPresetManager &OutManager) const;
		FPresetManager PresetManager;

		// Data Access
		const FArchitecturalMaterial *GetArchitecturalMaterialByKey(const FName &name) const;
		const FCustomColor *GetCustomColorByKey(const FName &Key) const;
		const FPortalPart *GetPortalPartByKey(const FName &Key) const;
		const FSimpleMeshRef *GetSimpleMeshByKey(const FName &Key) const;
		const FRoomConfiguration *GetRoomConfigByKey(const FName &Key) const;
		const FStaticIconTexture *GetStaticIconTextureByKey(const FName &Key) const;

		bool ParseColorFromField(FCustomColor &OutColor, const FString &Field);
		bool ParsePortalConfigDimensionSets(FName configKey, const FString &typeString, const TArray<FString> &setStrings, TArray<FPortalConfigDimensionSet> &outSets);


		TArray<FString> GetDebugInfo();
	};
}