// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "AdjustmentHandleAssetData.generated.h"

UCLASS(BlueprintType)
class MODUMATE_API UAdjustmentHandleAssetData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* GenericPointStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* GenericArrowStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* RotateStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* InvertStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* TransvertStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* JustificationRootStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* RoofEditEdgeStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* RoofCreateFacesStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* RoofRetractFacesStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* CabinetFrontFaceStyle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class USlateWidgetStyleAsset* PortalJustifyStyle;
};
