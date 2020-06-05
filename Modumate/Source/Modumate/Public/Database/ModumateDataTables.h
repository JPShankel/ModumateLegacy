// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Runtime/Engine/Classes/Engine/DataTable.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMLegacyPortals.h"
#include "ModumateDataTables.generated.h"


USTRUCT()
struct FFFEPartTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FSoftObjectPath AssetFilePath;

	UPROPERTY(EditAnywhere)
	FString LightConfigKey;

	UPROPERTY(EditAnywhere)
	FVector LightLocation;

	UPROPERTY(EditAnywhere)
	FRotator LightRotation;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath PerimeterMeshPath;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath InteriorMeshPath;
};

USTRUCT()
struct FFFEAssemblyTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	TArray<FString> FFEKeys;

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSubcategories;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedResources;

	UPROPERTY(EditAnywhere)
	FVector NormalVector;

	UPROPERTY(EditAnywhere)
	FVector TangentVector;
};

USTRUCT()
struct FMeshTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FSoftObjectPath AssetFilePath;
};

USTRUCT()
struct FLightTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	float LightIntensity;

	UPROPERTY(EditAnywhere)
	FLinearColor LightColor;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath LightProfilePath;

	UPROPERTY(EditAnywhere)
	bool bAsSpotLight;
};

USTRUCT()
struct FColorTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FName Library;

	UPROPERTY(EditAnywhere)
	FName Category;

	UPROPERTY(EditAnywhere)
	FString Hex;

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FText InspiredByProduct;
};

USTRUCT()
struct FMaterialTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FName Category;

	UPROPERTY(EditAnywhere)
	FString AssetName;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath EngineMaterialPath;

	UPROPERTY(EditAnywhere)
	FString DefaultBaseColor;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedBaseColors;

	UPROPERTY(EditAnywhere)
	bool SupportsRGBColorPicker;

	UPROPERTY(EditAnywhere)
	float TextureTilingSize;

	UPROPERTY(EditAnywhere)
	float HSVRangeWhenTiled;

	UPROPERTY(EditAnywhere)
	FString DefaultSurfaceTreatment;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSurfaceTreatments;
};

USTRUCT()
struct FPortalPartTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	TArray<EPortalSlotType> SupportedSlotTypes;

	UPROPERTY(EditAnywhere)
	FString Mesh;

	UPROPERTY(EditAnywhere)
	float NativeSizeX;

	UPROPERTY(EditAnywhere)
	float NativeSizeY;

	UPROPERTY(EditAnywhere)
	float NativeSizeZ;

	UPROPERTY(EditAnywhere)
	float NineSliceX1;

	UPROPERTY(EditAnywhere)
	float NineSliceX2;

	UPROPERTY(EditAnywhere)
	float NineSliceZ1;

	UPROPERTY(EditAnywhere)
	float NineSliceZ2;

	UPROPERTY(EditAnywhere)
	TMap<FName, float> ConfigurationDimensions;

	UPROPERTY(EditAnywhere)
	TArray<FName> SupportedMaterialChannels;
};

USTRUCT()
struct FPortalConfigurationTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();
	
	// Values should be EPortalFunction with prefixes.
	UPROPERTY(EditAnywhere)
	FString SupportedSubcategory;

	UPROPERTY(EditAnywhere)
	FText ConfigurationDisplayName;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedWidths;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedHeights;

	// Values should be EPortalSlotType or reference planes.
	UPROPERTY(EditAnywhere)
	FString SlotType;

	UPROPERTY(EditAnywhere)
	FString LocationX;

	UPROPERTY(EditAnywhere)
	FString LocationY;

	UPROPERTY(EditAnywhere)
	FString LocationZ;

	UPROPERTY(EditAnywhere)
	FString SizeX;

	UPROPERTY(EditAnywhere)
	FString SizeY;

	UPROPERTY(EditAnywhere)
	FString SizeZ;

	UPROPERTY(EditAnywhere)
	float RotationX;

	UPROPERTY(EditAnywhere)
	float RotationY;

	UPROPERTY(EditAnywhere)
	float RotationZ;

	UPROPERTY(EditAnywhere)
	bool FlipX;

	UPROPERTY(EditAnywhere)
	bool FlipY;

	UPROPERTY(EditAnywhere)
	bool FlipZ;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath CraftingIconAssetFilePath;
};

USTRUCT()
struct FPortalPartOptionsSetDataTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
		TArray<FString> SupportedSubcategories;

	UPROPERTY(EditAnywhere)
		TArray<FName> ConfigurationWhitelist;

	UPROPERTY(EditAnywhere)
		FText DisplayName;

	UPROPERTY(EditAnywhere)
		FName Frame;

	UPROPERTY(EditAnywhere)
		FName Mullion;

	UPROPERTY(EditAnywhere)
		FName Panel;

	UPROPERTY(EditAnywhere)
		FName Screen;

	UPROPERTY(EditAnywhere)
		FName Handle;

	UPROPERTY(EditAnywhere)
		FName Transom;

	UPROPERTY(EditAnywhere)
		FName Sidelite;

	UPROPERTY(EditAnywhere)
		FName Track;

	UPROPERTY(EditAnywhere)
		FName Caster;
};


USTRUCT() // To be deprecated with DDL 1.0, used for DDL 1.0 decision trees, renamed below
struct FDecisionTreeDataTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FString C0;
	
	UPROPERTY(EditAnywhere)
	FString C1;
	
	UPROPERTY(EditAnywhere)
	FString C2;
	
	UPROPERTY(EditAnywhere)
	FString C3;
	
	UPROPERTY(EditAnywhere)
	FString C4;
	
	UPROPERTY(EditAnywhere)
	FString C5;
	
	UPROPERTY(EditAnywhere)
	FString C6;
	
	UPROPERTY(EditAnywhere)
	FString C7;

	UPROPERTY(EditAnywhere)
	FString C8;

	UPROPERTY(EditAnywhere)
	FString C9;
};

USTRUCT() 
struct FTenColumnTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
		FString C0;

	UPROPERTY(EditAnywhere)
		FString C1;

	UPROPERTY(EditAnywhere)
		FString C2;

	UPROPERTY(EditAnywhere)
		FString C3;

	UPROPERTY(EditAnywhere)
		FString C4;

	UPROPERTY(EditAnywhere)
		FString C5;

	UPROPERTY(EditAnywhere)
		FString C6;

	UPROPERTY(EditAnywhere)
		FString C7;

	UPROPERTY(EditAnywhere)
		FString C8;

	UPROPERTY(EditAnywhere)
		FString C9;
};

USTRUCT()
struct FCraftingSubcategoryDataTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FString SheetName;

	UPROPERTY(EditAnywhere)
	TArray<EToolMode> ToolMode;

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FString IDCodeLine1;

	UPROPERTY(EditAnywhere)
	ELayerFunction LayerFunction;

	UPROPERTY(EditAnywhere)
	ELayerFormat Format;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath CraftingIconAssetFilePath;
};

USTRUCT()
struct FLayerThicknessOptionSetDataTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSubcategories;
	
	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FString Thickness;

	UPROPERTY(EditAnywhere)
	FString ThicknessUnits;

	UPROPERTY(EditAnywhere)
	FString IDCodeLine2;
};

USTRUCT()
struct FDimensionalOptionSetDataTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FName DimensionalOptionType;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSubcategories;

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath CraftingIconAssetFilePath;

	UPROPERTY(EditAnywhere)
	float Length;

	UPROPERTY(EditAnywhere)
	FString LUnits;

	UPROPERTY(EditAnywhere)
	float Width;

	UPROPERTY(EditAnywhere)
	FString WUnits;

	UPROPERTY(EditAnywhere)
	float Height;

	UPROPERTY(EditAnywhere)
	FString HUnits;

	UPROPERTY(EditAnywhere)
	float Depth;

	UPROPERTY(EditAnywhere)
	FString DUnits;

	UPROPERTY(EditAnywhere)
	float Thickness;

	UPROPERTY(EditAnywhere)
	FString TUnits;

	UPROPERTY(EditAnywhere)
	float BevelWidth;

	UPROPERTY(EditAnywhere)
	FString BevelUnit;

	UPROPERTY(EditAnywhere)
	FString IDCodeLine2;
};

USTRUCT()
struct FMaterialsAndColorsOptionSetDataTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedOptionSets;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSubcategories;

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FString Material;

	UPROPERTY(EditAnywhere)
	FString BaseColor;

	UPROPERTY(EditAnywhere)
	FString BaseColorHuman;
};

USTRUCT()
struct FPatternOptionSetDataTable : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();
	
	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSubcategories;

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath CraftingIconAssetFilePath;

	UPROPERTY(EditAnywhere)
	FString ModuleCount;

	UPROPERTY(EditAnywhere)
	FString Extents;

	UPROPERTY(EditAnywhere)
	FString PatternThickness;

	UPROPERTY(EditAnywhere)
	FString Gap;

	UPROPERTY(EditAnywhere)
	FString Module1Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module2Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module3Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module4Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module5Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module6Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module7Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module8Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module9Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module10Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module11Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module12Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module13Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module14Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module15Dimensions;

	UPROPERTY(EditAnywhere)
	FString Module16Dimensions;
};

USTRUCT()
struct FDrawingScalesTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSubcategories;

	UPROPERTY(EditAnywhere)
	FString DisplayName;

	UPROPERTY(EditAnywhere)
	int32 PaperScale;

	UPROPERTY(EditAnywhere)
	int32 CorrespondingProjectScale;
};

USTRUCT()
struct FProfileOptionSetTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath AssetPath;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath CraftingIconAssetFilePath;

	UPROPERTY(EditAnywhere)
	TArray<FString> SupportedSubcategories;
};

USTRUCT(BlueprintType)
struct FRoomConfigurationTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FName UseGroupCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText UseGroupType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	float OccupantLoadFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	EAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText LoadFactorSpecialCalc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FString HexValue;
};

