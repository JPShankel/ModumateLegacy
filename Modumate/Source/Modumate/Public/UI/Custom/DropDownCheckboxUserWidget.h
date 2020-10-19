// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DropDownCheckboxUserWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UDropDownCheckboxUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDropDownCheckboxUserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TooltipID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateComboBoxString *ModumateComboBox;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnDropDownSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	UWidget* OnTooltipWidget();
};
