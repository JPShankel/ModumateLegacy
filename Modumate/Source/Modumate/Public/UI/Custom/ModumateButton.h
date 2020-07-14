// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "ModumateButton.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateButton : public UButton
{
	GENERATED_BODY()

public:

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool OverrideImageSize = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class USlateWidgetStyleAsset *CustomButtonStyle;

	bool ApplyCustomStyle();
};
