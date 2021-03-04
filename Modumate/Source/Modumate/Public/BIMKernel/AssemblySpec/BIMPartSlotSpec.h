// Copyright 2018 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateUnits.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"

#include "BIMPartSlotSpec.generated.h"

class FModumateDatabase;

struct FBIMPresetInstance;

UENUM()
enum class EPartSlotDimensionUIType : uint8
{
	Major = 0,
	Minor,
	Preview,
	Hidden
};

USTRUCT()
struct MODUMATE_API FPartNamedDimension
{
	GENERATED_BODY()

	UPROPERTY()
	EPartSlotDimensionUIType UIType;

	UPROPERTY()
	FModumateUnitValue DefaultValue;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FText Description;
};

/*
An assembly part slot contains a mesh and local transform
TODO: transforms will be derived from hosting plane/moi parameters...to be hard coded initially
*/
USTRUCT()
struct MODUMATE_API FBIMPartSlotSpec
{
	GENERATED_BODY()

	friend struct FBIMAssemblySpec;
private:

	UPROPERTY()
	FBIMPropertySheet Properties;

	EBIMResult BuildFromProperties(const FModumateDatabase& InDB);

public:

#if WITH_EDITOR
	EBIMValueScope DEBUGNodeScope;
#endif

	UPROPERTY()
	FGuid PresetGUID;

	UPROPERTY()
	FGuid SlotGUID;

	UPROPERTY()
	FVectorExpression Translation;

	UPROPERTY()
	FVectorExpression Size;

	UPROPERTY()
	FVectorExpression Orientation;

	UPROPERTY()
	TArray<bool> Flip = { false,false,false };

	UPROPERTY()
	int32 ParentSlotIndex = 0;

	UPROPERTY()
	FBIMTagPath NodeCategoryPath;

	UPROPERTY()
	FString SlotID;

	UPROPERTY()
	FArchitecturalMesh Mesh;
	
	UPROPERTY()
	TMap<FName, FArchitecturalMaterial> ChannelMaterials;

	UPROPERTY()
	TMap<FString, FModumateUnitValue> NamedDimensionValues;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;

	void GetNamedDimensionValuesFromPreset(const FBIMPresetInstance* Preset);

	static TMap<FString, FPartNamedDimension> NamedDimensionMap;
	static bool TryGetDefaultNamedDimension(const FString& Name, FModumateUnitValue& OutVal);
};
