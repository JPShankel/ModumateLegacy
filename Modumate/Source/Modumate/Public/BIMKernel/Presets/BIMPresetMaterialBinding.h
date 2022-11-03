// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "CustomData/CustomDataWebConvertable.h"

#include "BIMPresetMaterialBinding.generated.h"

UENUM()
enum class EMaterialChannelFields : uint8
{
	None,
	AppliesToChannel,
	InnerMaterial,
	SurfaceMaterial,
	ColorTint,
	ColorTintVariation,
	Error = 255
};

class FModumateDatabase;
struct FArchitecturalMaterial;
class FBIMPresetCollectionProxy;

USTRUCT()
struct MODUMATE_API FBIMPresetMaterialBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName Channel;

	UPROPERTY()
	FGuid InnerMaterialGUID;

	UPROPERTY()
	FGuid SurfaceMaterialGUID;

	UPROPERTY()
	FString ColorHexValue;

	UPROPERTY()
	float ColorTintVariationPercent = 0;

	EBIMResult GetEngineMaterial(const FBIMPresetCollectionProxy& PresetCollection, FArchitecturalMaterial& OutMaterial) const;

	bool operator==(const FBIMPresetMaterialBinding& RHS) const;
	bool operator!=(const FBIMPresetMaterialBinding& RHS) const;
};

struct FBIMPresetForm;
USTRUCT()
struct MODUMATE_API FBIMPresetMaterialBindingSet : public FCustomDataWebConvertable
{
	GENERATED_BODY()
	friend bool operator==(const FBIMPresetMaterialBindingSet& Lhs, const FBIMPresetMaterialBindingSet& RHS)
	{
		return Lhs.MaterialBindings == RHS.MaterialBindings;
	}

	friend bool operator!=(const FBIMPresetMaterialBindingSet& Lhs, const FBIMPresetMaterialBindingSet& RHS)
	{
		return !(Lhs == RHS);
	}

	UPROPERTY()
	TArray<FBIMPresetMaterialBinding> MaterialBindings;

	EBIMResult SetFormElements(FBIMPresetForm& RefForm) const;

	virtual void ConvertToWebPreset(FBIMWebPreset& OutPreset) override;
	virtual void ConvertFromWebPreset(const FBIMWebPreset& InPreset) override;

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Material);
	}

protected:
	virtual UStruct* VirtualizedStaticStruct() override;
};