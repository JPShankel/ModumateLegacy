// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateUnits.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Database/ModumateObjectEnums.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMLegacyPattern.generated.h"

struct FBIMPresetInstance;

USTRUCT()
struct MODUMATE_API FLayerPatternModule
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Key;

	UPROPERTY()
	FText DisplayName = FText::GetEmpty();

	UPROPERTY()
	TArray<ELayerFormat> SupportedFormats;

	UPROPERTY()
	FVector ModuleExtents = FVector::ZeroVector;

	UPROPERTY()
	float BevelWidthCentimeters = 0.0f;

	UPROPERTY()
	FArchitecturalMaterial Material;

	FGuid UniqueKey() const { return Key; }
};

USTRUCT()
struct MODUMATE_API FLayerPatternGap
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Key;

	UPROPERTY()
	FText DisplayName = FText::GetEmpty();

	UPROPERTY()
	FVector2D GapExtents = FVector2D::ZeroVector;

	UPROPERTY()
	FArchitecturalMaterial Material;

	UPROPERTY()
	TArray<ELayerFormat> SupportedFormats;

	FGuid UniqueKey() const { return Key; }
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

	UPROPERTY()
	FGuid Key;

	UPROPERTY()
	FText DisplayName = FText::GetEmpty();

	UPROPERTY()
	TArray<ELayerFormat> SupportedFormats;

	UPROPERTY()
	int32 ModuleCount = 0;

	UPROPERTY()
	FName ThicknessDimensionPropertyName;

	UPROPERTY()
	FString ParameterizedExtents;

	FVector CachedExtents { ForceInit };
	
	UPROPERTY()
	TArray<FPatternModuleTemplate> ParameterizedModuleDimensions;

	UPROPERTY()
	FLayerPatternGap DefaultGap;

	UPROPERTY()
	TArray<FLayerPatternModule> DefaultModules;

	void InitFromCraftingPreset(const FBIMPresetInstance& Preset);

	FGuid UniqueKey() const { return Key; }
};

// TODO: add to BIM lighting kernel when it exists 
USTRUCT()
struct FLightConfiguration
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid PresetGUID;

	UPROPERTY()
	float LightIntensity = 0.f;

	UPROPERTY()
	FLinearColor LightColor = FLinearColor::White;

	UPROPERTY()
	TWeakObjectPtr<UTextureLightProfile> LightProfile = nullptr;

	UPROPERTY()
	bool bAsSpotLight = false;

	FGuid UniqueKey() const { return PresetGUID; }
};
