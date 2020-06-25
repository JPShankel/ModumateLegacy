// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "ModumateEditableTextBox.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateEditableTextBox : public UEditableTextBox
{
	GENERATED_BODY()

public:

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		class USlateWidgetStyleAsset *CustomEditableTextBox;

	bool ApplyCustomStyle();
};
