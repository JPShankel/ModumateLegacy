// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Slider.h"
#include "ModumateSlider.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateSlider : public USlider
{
	GENERATED_BODY()

public:

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class USlateWidgetStyleAsset *CustomSliderStyleAsset;

	bool ApplyCustomStyle();
};
