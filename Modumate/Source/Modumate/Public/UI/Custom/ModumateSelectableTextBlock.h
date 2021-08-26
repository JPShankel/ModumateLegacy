// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/MultiLineEditableTextBox.h"

#include "ModumateSelectableTextBlock.generated.h"


/**
 *
 */
UCLASS()
class MODUMATE_API UModumateSelectableTextBlock : public UMultiLineEditableTextBox
{
	GENERATED_BODY()

public:

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class USlateWidgetStyleAsset* CustomEditableText;

	bool ApplyCustomStyle();

};
