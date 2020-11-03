// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/CheckBox.h"
#include "ModumateCheckBox.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateCheckBox : public UCheckBox
{
	GENERATED_BODY()

public:

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class USlateWidgetStyleAsset* CustomCheckbox;

	bool ApplyCustomStyle();
};
