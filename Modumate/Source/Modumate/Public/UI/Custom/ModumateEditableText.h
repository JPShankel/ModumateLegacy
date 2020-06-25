// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableText.h"
#include "ModumateEditableText.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateEditableText : public UEditableText
{
	GENERATED_BODY()

public:

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		class USlateWidgetStyleAsset *CustomEditableText;

	bool ApplyCustomStyle();
};
