// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMKey.h"
#include "Runtime/Engine/Classes/Materials/Material.h"

#include "ModumateArchitecturalMaterial.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FCustomColor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FBIMKey Key;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FColor Color = FColor::White;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FBIMKey Library;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText DisplayName;

	FBIMKey CombinedKey;
	bool bValid = false;

	FCustomColor()
		: bValid(false)
	{ }

	FCustomColor(const FBIMKey& InKey, FColor InColor, const FBIMKey& InLibrary, const FText& InDisplayName)
		: Key(InKey)
		, Color(InColor)
		, Library(InLibrary)
		, DisplayName(InDisplayName)
		, CombinedKey(Library + Key)
		, bValid(true)
	{ }

	FBIMKey UniqueKey() const { return CombinedKey; }
	bool IsValid() const { return bValid; }

	FString ToString() const
	{
		return IsValid() ? (CombinedKey.IsNone() ? Color.ToHex() : CombinedKey.StringValue) : FString();
	}
};

/*
Materials like gypsum, wood, cement, etc located in ShoppingData/Materials
*/
USTRUCT()
struct FArchitecturalMaterial
{
	GENERATED_BODY()

	FBIMKey Key;
	FText DisplayName = FText::GetEmpty();

	TWeakObjectPtr<UMaterialInterface> EngineMaterial = nullptr;

	FCustomColor DefaultBaseColor;
	TArray<FCustomColor> SupportedBaseColors;
	bool bSupportsColorPicker = false;
	float UVScaleFactor = 1.0f;
	float HSVRangeWhenTiled = 0.0f;

	FBIMKey UniqueKey() const { return Key; }

	// Whether this material has been created with real data.
	// TODO: make this more accurate once the underlying materials are lazy-loaded.
	bool IsValid() const { return EngineMaterial.IsValid(); }
};

USTRUCT(BlueprintType)
struct MODUMATE_API FStaticIconTexture
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FBIMKey Key;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText DisplayName = FText::GetEmpty();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TWeakObjectPtr<UTexture2D> Texture;

	FBIMKey UniqueKey() const { return Key; }

	// TODO: Like FArchitecturalMaterial, check if texture will be lazy-loaded.
	bool IsValid() const { return Texture.IsValid(); }
};