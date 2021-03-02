// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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
	FString ColorTintVariationHexValue;

	bool operator==(const FBIMPresetMaterialBinding& RHS) const;
	bool operator!=(const FBIMPresetMaterialBinding& RHS) const;
};

USTRUCT()
struct MODUMATE_API FBIMPresetMaterialBindingSet
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBIMPresetMaterialBinding> MaterialBindings;
};