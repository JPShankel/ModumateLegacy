// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "ModumateComboBoxString.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UModumateComboBoxString : public UComboBoxString
{
	GENERATED_BODY()

public:
	UModumateComboBoxString(const FObjectInitializer& ObjectInitializer);

	//Called during widget compile
	virtual void SynchronizeProperties() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class USlateWidgetStyleAsset *ComboBoxWidgetStyle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class USlateWidgetStyleAsset *ComboBoxItemStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateComboBoxStringItem> ItemWidgetClass;

	UFUNCTION()
	UWidget* OnComboBoxGenerateWidget(FString SelectedItem);

	bool ApplyCustomStyle();
};
