// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Materials/Material.h"

#include "ModumateArchitecturalMaterial.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FCustomColor
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FName Key;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FColor Color = FColor::White;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FName Library;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FText DisplayName;

	FString CombinedKey = FString();
	bool bValid = false;

	FCustomColor()
		: bValid(false)
	{ }

	FCustomColor(FName InKey, FColor InColor, FName InLibrary, FText InDisplayName)
		: Key(InKey)
		, Color(InColor)
		, Library(InLibrary)
		, DisplayName(InDisplayName)
		, CombinedKey(Library.ToString() + Key.ToString())
		, bValid(true)
	{ }

	FName UniqueKey() const { return *CombinedKey; }
	bool IsValid() const { return bValid; }

	FString ToString() const
	{
		return IsValid() ? (CombinedKey.IsEmpty() ? Color.ToHex() : CombinedKey) : FString();
	}
};

/*
Materials like gypsum, wood, cement, etc located in ShoppingData/Materials
*/
USTRUCT()
struct FArchitecturalMaterial
{
	GENERATED_USTRUCT_BODY();

	FName Key = FName();
	FText DisplayName = FText::GetEmpty();

	TWeakObjectPtr<UMaterialInterface> EngineMaterial = nullptr;

	FCustomColor DefaultBaseColor;
	TArray<FCustomColor> SupportedBaseColors;
	bool bSupportsColorPicker = false;
	float UVScaleFactor = 1.0f;
	float HSVRangeWhenTiled = 0.0f;

	FName UniqueKey() const { return Key; }

	// Whether this material has been created with real data.
	// TODO: make this more accurate once the underlying materials are lazy-loaded.
	bool IsValid() const { return EngineMaterial.IsValid(); }
};

USTRUCT(BlueprintType)
struct MODUMATE_API FStaticIconTexture
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FName Key;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FText DisplayName = FText::GetEmpty();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		TWeakObjectPtr<UTexture2D> Texture;

	FName UniqueKey() const { return Key; }

	// TODO: Like FArchitecturalMaterial, check if texture will be lazy-loaded.
	bool IsValid() const { return Texture.IsValid(); }
};