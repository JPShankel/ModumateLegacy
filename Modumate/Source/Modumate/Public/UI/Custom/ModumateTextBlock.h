// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "ModumateTextBlock.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class USlateWidgetStyleAsset *CustomTextBlockStyle;

	bool ApplyCustomStyle();

};
