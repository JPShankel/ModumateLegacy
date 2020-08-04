// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WidgetClassAssetData.generated.h"

UCLASS(BlueprintType)
class MODUMATE_API UWidgetClassAssetData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	TSubclassOf<class UHUDDrawWidget> HUDDrawWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	TSubclassOf<class UDimensionWidget> DimensionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	TSubclassOf<class UAdjustmentHandleWidget> AdjustmentHandleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Handles)
	TSubclassOf<class URoofPerimeterPropertiesWidget> RoofPerimeterPropertiesClass;
};
