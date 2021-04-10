// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMKey.h"
#include "Runtime/Engine/Classes/Materials/Material.h"

#include "ModumateArchitecturalMaterial.generated.h"



/*
Materials like gypsum, wood, cement, etc located in ShoppingData/Materials
*/
USTRUCT()
struct MODUMATE_API FArchitecturalMaterial
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Key;

	UPROPERTY()
	FText DisplayName = FText::GetEmpty();

	UPROPERTY()
	TWeakObjectPtr<UMaterialInterface> EngineMaterial = nullptr;

	UPROPERTY()
	FColor Color = FColor::White;

	UPROPERTY()
	float UVScaleFactor = 1.0f;

	UPROPERTY()
	float HSVRangeWhenTiled = 0.0f;

	FGuid UniqueKey() const { return Key; }

	// Whether this material has been created with real data.
	// TODO: make this more accurate once the underlying materials are lazy-loaded.
	bool IsValid() const { return EngineMaterial.IsValid(); }
};

USTRUCT(BlueprintType)
struct MODUMATE_API FStaticIconTexture
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid Key;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText DisplayName = FText::GetEmpty();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TWeakObjectPtr<UTexture2D> Texture;

	FGuid UniqueKey() const { return Key; }

	// TODO: Like FArchitecturalMaterial, check if texture will be lazy-loaded.
	bool IsValid() const { return Texture.IsValid(); }
};
