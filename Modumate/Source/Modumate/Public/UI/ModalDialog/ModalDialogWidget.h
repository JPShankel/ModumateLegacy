// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ModalDialogWidget.generated.h"

/**
 *
 */

UENUM()
enum class EModalButtonStyle : uint8
{
	Default,
	Green,
	Red,
	None
};

USTRUCT()
struct MODUMATE_API FModalButtonParam
{
	GENERATED_BODY()

	FModalButtonParam() {}
	FModalButtonParam(EModalButtonStyle InModalButtonStyle, const FText& InButtonText, const TFunction<void()>& InCallBack) :
		ModalButtonStyle(InModalButtonStyle), ButtonText(InButtonText), CallbackTask(InCallBack) {}

	EModalButtonStyle ModalButtonStyle = EModalButtonStyle::Default;
	FText ButtonText = FText::GetEmpty();
	TFunction<void()> CallbackTask = nullptr;
	FMargin ButtonMargin = FMargin(8.f, 0.f, 8.f, 0.f);
};

UCLASS()
class MODUMATE_API UModalDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModalDialogWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	void HideAllWidgets();
	void EnableInputHandler(bool bEnable);

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UAlertAccountDialogWidget* AlertAccountDialogWidgetBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UAlertAccountDialogWidget* UploadOfflineProjectDialogWidgetBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UAlertAccountDialogWidget* UploadOfflineDoneDialogWidgetBP;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class URichTextBlock* RichTextBlock_Header;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class URichTextBlock* RichTextBlock_Body;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox* HorizontalBox_Buttons;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USizeBox* SizeBox_Dialog;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBackgroundBlur* InputBlocker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateButtonUserWidget> DefaultButtonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateButtonUserWidget> GreenButtonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateButtonUserWidget> RedButtonClass;


	static const FText DefaultTitleText;

	void ShowStatusDialog(const FText& TitleText, const FText& StatusText, bool bAllowDismiss);
	void ShowAlertAccountDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback);
	void ShowUploadOfflineProjectDialog(const FText& TitleText, const FText& StatusText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback);
	void ShowUploadOfflineDoneDialog(const FText& TitleText, const FText& StatusText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback);


	void CreateModalDialog(const FText& HeaderText, const FText& BodyText, const TArray<FModalButtonParam>& ModalButtonParam);
	void CloseModalDialog();
	TSubclassOf<class UModumateButtonUserWidget> ButtonClassSelector(EModalButtonStyle Style) const;
};
