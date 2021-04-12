// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "AlertAccountDialogWidget.generated.h"

/**
 *
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAlertAccountPressedConfirm);

UCLASS()
class MODUMATE_API UAlertAccountDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAlertAccountDialogWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* AlertTextBlock;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonInfoLink;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonConfirm;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDismiss;

	UPROPERTY(BlueprintAssignable)
	FOnAlertAccountPressedConfirm OnPressedConfirm;

	void ShowDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback, bool bShowLinkButton = true);

protected:

	UFUNCTION()
	void OnReleaseButtonInfoLink();

	UFUNCTION()
	void OnReleaseButtonConfirm();

	UFUNCTION()
	void OnReleaseButtonDismiss();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ButtonInfoLinkURL;

	TFunction<void()> ConfirmCallback;
};
