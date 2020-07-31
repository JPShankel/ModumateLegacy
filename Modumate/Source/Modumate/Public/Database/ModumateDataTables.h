// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Runtime/Engine/Classes/Engine/DataTable.h"
#include "Database/ModumateObjectEnums.h"
#include "ModumateDataTables.generated.h"


USTRUCT()
struct FMeshTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FSoftObjectPath AssetFilePath;
};

USTRUCT()
struct FLightTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float LightIntensity;

	UPROPERTY(EditAnywhere)
	FLinearColor LightColor;

	UPROPERTY(EditAnywhere)
	FSoftObjectPath LightProfilePath;

	UPROPERTY(EditAnywhere)
	bool bAsSpotLight;
};

USTRUCT()
struct FColorTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName Library;

	UPROPERTY(EditAnywhere)
	FName Category;

	UPROPERTY(EditAnywhere)
	FString Hex;

	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FText InspiredByProduct;
};

USTRUCT()
struct FRoomConfigurationTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FName UseGroupCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText UseGroupType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	float OccupantLoadFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	EAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText LoadFactorSpecialCalc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FString HexValue;
};

