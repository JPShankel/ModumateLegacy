// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateUnits.h"
#include "Database/ModumateObjectEnums.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMLegacyPattern.generated.h"

namespace Modumate
{
	struct FCraftingPatternOption;
	struct FCraftingDimensionalOption;
}

USTRUCT()
struct FLayerPatternModule
{
	GENERATED_USTRUCT_BODY();

	FName Key = FName();
	FText DisplayName = FText::GetEmpty();

	TArray<ELayerFormat> SupportedFormats;

	FVector ModuleExtents = FVector::ZeroVector;

	Modumate::Units::FUnitValue BevelWidth = Modumate::Units::FUnitValue::WorldCentimeters(0);

	FArchitecturalMaterial Material;

	void InitFromCraftingParameters(const Modumate::FModumateFunctionParameterSet &params);

	FName UniqueKey() const { return Key; }
};

USTRUCT()
struct FLayerPatternGap
{
	GENERATED_USTRUCT_BODY();

	FName Key = FName();
	FText DisplayName = FText::GetEmpty();

	FVector2D GapExtents;

	FArchitecturalMaterial Material;
	FCustomColor BaseColor;
	TArray<ELayerFormat> SupportedFormats;

	void InitFromCraftingParameters(const Modumate::FModumateFunctionParameterSet &params);

	FName UniqueKey() const { return Key; }
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
struct FLayerPattern
{
	GENERATED_USTRUCT_BODY();

	FName Key = FName();
	FText DisplayName = FText::GetEmpty();
	TArray<ELayerFormat> SupportedFormats;
	int32 ModuleCount = 1;

	FString ParameterizedExtents;
	FString ParameterizedThickness;
	TArray<FPatternModuleTemplate> ParameterizedModuleDimensions;

	FLayerPatternGap DefaultGap;
	TArray<FLayerPatternModule> DefaultModules;

	void InitFromCraftingParameters(const Modumate::FModumateFunctionParameterSet &params);

	FName UniqueKey() const { return Key; }
};