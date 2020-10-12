// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateUnits.h"
#include "BIMKernel/BIMProperties.h"
#include "Database/ModumateObjectEnums.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMLegacyPattern.generated.h"

class FBIMPreset;

USTRUCT()
struct MODUMATE_API FLayerPatternModule
{
	GENERATED_BODY()

	FBIMKey Key;
	FText DisplayName = FText::GetEmpty();

	TArray<ELayerFormat> SupportedFormats;

	FVector ModuleExtents = FVector::ZeroVector;

	Modumate::Units::FUnitValue BevelWidth = Modumate::Units::FUnitValue::WorldCentimeters(0);

	FArchitecturalMaterial Material;

	FBIMKey UniqueKey() const { return Key; }
};

USTRUCT()
struct MODUMATE_API FLayerPatternGap
{
	GENERATED_BODY()

	FBIMKey Key;
	FText DisplayName = FText::GetEmpty();

	FVector2D GapExtents = FVector2D::ZeroVector;

	FArchitecturalMaterial Material;
	FCustomColor BaseColor;
	TArray<ELayerFormat> SupportedFormats;

	FBIMKey UniqueKey() const { return Key; }
};

USTRUCT()
struct MODUMATE_API FPatternModuleParams
{
	GENERATED_BODY()

	UPROPERTY()
	FLinearColor Dimensions = FLinearColor::Transparent;

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;

	UPROPERTY()
	FLinearColor TileTexDetails = FLinearColor::Transparent;

	UPROPERTY()
	FLinearColor TileShapeDetails = FLinearColor::Transparent;

	UPROPERTY()
	UTexture* BaseColorTex;

	UPROPERTY()
	UTexture* MRSATex;

	UPROPERTY()
	UTexture* NormalTex;
};

USTRUCT()
struct MODUMATE_API FPatternModuleTemplate
{
	GENERATED_BODY()

	FPatternModuleTemplate() {};
	FPatternModuleTemplate(const FString &DimensionStringsCombined);

	UPROPERTY()
	FString ModuleXExpr;

	UPROPERTY()
	FString ModuleYExpr;

	UPROPERTY()
	int32 ModuleDefIndex = 0;

	UPROPERTY()
	FString ModuleWidthExpr;

	UPROPERTY()
	FString ModuleHeightExpr;

	UPROPERTY()
	FString OriginalString;

	UPROPERTY()
	bool bIsValid = false;
};

USTRUCT()
struct MODUMATE_API FLayerPattern
{
	GENERATED_BODY()

	FBIMKey Key;
	FText DisplayName = FText::GetEmpty();
	TArray<ELayerFormat> SupportedFormats;
	int32 ModuleCount = 0;

	FString ParameterizedExtents;
	FString ParameterizedThickness;
	TArray<FPatternModuleTemplate> ParameterizedModuleDimensions;

	FLayerPatternGap DefaultGap;
	TArray<FLayerPatternModule> DefaultModules;

	void InitFromCraftingPreset(const FBIMPreset& Preset);

	FBIMKey UniqueKey() const { return Key; }
};

// TODO: add to BIM lighting kernel when it exists 
USTRUCT()
struct FLightConfiguration
{
	GENERATED_BODY()

	FBIMKey Key;
	float LightIntensity = 0.f;
	FLinearColor LightColor = FLinearColor::White;
	TWeakObjectPtr<UTextureLightProfile> LightProfile = nullptr;
	bool bAsSpotLight = false;

	FBIMKey UniqueKey() const { return Key; }
};
