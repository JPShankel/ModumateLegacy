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
struct MODUMATE_API FBIMPresetMaterialChannelBinding
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

	bool operator==(const FBIMPresetMaterialChannelBinding& RHS) const;
	bool operator!=(const FBIMPresetMaterialChannelBinding& RHS) const;
};