// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateUnits.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Objects/ModumateObjectEnums.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "Engine/TextureLightProfile.h"
#include "BIMKernel/Presets/CustomData/CustomDataWebConvertable.h"
#include "BIMLegacyPattern.generated.h"

struct FBIMPresetInstance;

USTRUCT()
struct MODUMATE_API FLayerPatternModule
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Key;

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
	FVector2D GapExtents = FVector2D::ZeroVector;

	UPROPERTY()
	FArchitecturalMaterial Material;

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
	UTexture* BaseColorTex = nullptr;

	UPROPERTY()
	UTexture* MRSATex = nullptr;

	UPROPERTY()
	UTexture* NormalTex = nullptr;
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
struct FLightConfiguration : public FCustomDataWebConvertable
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid IESProfileGUID;

	UPROPERTY()
	float LightIntensity = 0.f;

	UPROPERTY()
	FColor LightColor = FColor::White;

	UPROPERTY()
	TWeakObjectPtr<UTextureLightProfile> LightProfile = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UTexture2D> LightProfileIcon = nullptr;

	UPROPERTY()
	bool bAsSpotLight = false;
	
	UPROPERTY()
	FString Name;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FVector Scale = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	float SourceRadius = 0.0f;

	FGuid UniqueKey() const { return IESProfileGUID; }

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::IESLight);
	};

	friend bool operator==(const FLightConfiguration& Lhs, const FLightConfiguration& RHS)
	{
		return Lhs.IESProfileGUID == RHS.IESProfileGUID
			&& Lhs.LightIntensity == RHS.LightIntensity
			&& Lhs.LightColor == RHS.LightColor
			&& Lhs.bAsSpotLight == RHS.bAsSpotLight
			&& Lhs.Name == RHS.Name
			&& Lhs.Location == RHS.Location
			&& Lhs.Scale == RHS.Scale
			&& Lhs.Rotation == RHS.Rotation
			&& Lhs.SourceRadius == RHS.SourceRadius;
	}

	friend bool operator!=(const FLightConfiguration& Lhs, const FLightConfiguration& RHS)
	{
		return !(Lhs == RHS);
	}


protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};