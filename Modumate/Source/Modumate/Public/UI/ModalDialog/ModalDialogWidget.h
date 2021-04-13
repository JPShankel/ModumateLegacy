// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ModalDialogWidget.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UModalDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModalDialogWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UAlertAccountDialogWidget* AlertAccountDialogWidgetBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UAlertAccountDialogWidget* DeletePresetDialogWidgetBP;

	void ShowAlertAccountDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback);
	void ShowDeletePresetDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback);

	void HideAllWidgets();
};
