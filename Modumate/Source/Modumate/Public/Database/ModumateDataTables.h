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
	float OccupantLoadFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	EAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText LoadFactorSpecialCalc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FString HexValue;
};

