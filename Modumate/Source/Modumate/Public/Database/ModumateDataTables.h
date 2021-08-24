// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Runtime/Engine/Classes/Engine/DataTable.h"
#include "Database/ModumateObjectEnums.h"
#include "ModumateDataTables.generated.h"


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
	float OccupantLoadFactor=0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	EAreaType AreaType=EAreaType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText LoadFactorSpecialCalc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FString HexValue;
};

USTRUCT()
struct FBIMSlotConfigDataTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Active;

	UPROPERTY()
	FString ID;

	UPROPERTY()
	FString ConfigName;

	UPROPERTY()
	FString SlotID;

	UPROPERTY()
	FString SupportedNCP;

	UPROPERTY()
	FString SlotName;

	UPROPERTY()
	FString LocationX;

	UPROPERTY()
	FString LocationY;

	UPROPERTY()
	FString LocationZ;

	UPROPERTY()
	FString SizeX;

	UPROPERTY()
	FString SizeY;

	UPROPERTY()
	FString SizeZ;

	UPROPERTY()
	FString RotationX;

	UPROPERTY()
	FString RotationY;

	UPROPERTY()
	FString RotationZ;

	UPROPERTY()
	FString FlipX;

	UPROPERTY()
	FString FlipY;

	UPROPERTY()
	FString FlipZ;
};

// Struct for the Tooltip data table
USTRUCT(BlueprintType)
struct MODUMATE_API FTooltipNonInputDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText TooltipTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText TooltipText;
};