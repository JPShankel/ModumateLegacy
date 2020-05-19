// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Runtime/Engine/Classes/Materials/Material.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateObjectEnums.h"
#include "ModumateShoppingItem.generated.h"

/*
Shopping items are used by blueprint to present database assets in the shopping UI
*/
USTRUCT(BlueprintType)
struct MODUMATE_API FShoppingItem
{
	GENERATED_BODY();

	FShoppingItem() { }
	FShoppingItem(bool bStartValid) : Valid(bStartValid) { }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	FString CraftingComments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	FString Type;

	// TODO: Change "Key" to use FName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	FString FormattedKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopping")
	float Thickness = 2.54f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopping")
	FVector Extents = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ELayerFormat Format = ELayerFormat::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ELayerFunction Function = ELayerFunction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPortalFunction PortalFunction = EPortalFunction::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	TWeakObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	TWeakObjectPtr<UStaticMesh> EngineMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shopping")
	TWeakObjectPtr<UMaterialInterface> EngineMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ModuleDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText PatternDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText GapDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString MaterialCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PresetCode;

	/*
	The following fields are used by the detail view of the assembly shopping dialog
	*/

	// Column 2 of the crafting decision tree spreadsheet is the high-level subcategory like "mass wall"
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString C2Subcategory = "C2 Subcategory";

	// Column 4 of the crafting decision tree spreadsheet is the subcategory type, like "mass wall - concrete"
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString C4Subcategory = "C4 Subcategory";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Valid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Locked = true;

	static const FShoppingItem ErrorItem;
};

USTRUCT(BlueprintType)
struct FShoppingComponentAttributes
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString MaterialKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ELayerFormat Format;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Thickness;
};

USTRUCT(BlueprintType)
struct FRoomConfigurationBlueprint
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	int32 ObjectID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FString RoomNumber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FColor RoomColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	float Area;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FName UseGroupCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText UseGroupType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	int32 OccupantsNumber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	float OccupantLoadFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	EAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText LoadFactorSpecialCalc;
};

USTRUCT(BlueprintType)
struct FCutPlaneParamBlueprint
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	int32 ObjectID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FString DisplayName = FString(TEXT("New Cut Plane"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	bool bVisiblity = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FVector Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FVector Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	FVector Size;

	// If true, menu will start initial behavior such as user naming process
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CutPlane")
	bool bRecentlyCreated = false;
};

USTRUCT(BlueprintType)
struct FScopeBoxParamBlueprint
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	int32 ObjectID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FString DisplayName = FString(TEXT("New Scope Box"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	bool bVisiblity = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FVector Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	FVector Extent;

	// If true, menu will start initial behavior such as user naming process
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ScopeBox")
	bool bRecentlyCreated = false;
};
