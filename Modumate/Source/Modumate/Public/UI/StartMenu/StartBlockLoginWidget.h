// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartBlockLoginWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UStartBlockLoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStartBlockLoginWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY()
	class UModumateGameInstance *ModumateGameInstance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString CreateAccountURL;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ForgotPasswordURL;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSlateColor SaveCredentialColor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonCreateAccount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonForgotPassword;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonLogin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBox *EmailBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBox *PasswordBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTextBlock *TextBlock_LoginAttemptMsg;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* SaveEmail;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* SaveCredentials;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void OnButtonReleasedCreateAccount();

	UFUNCTION()
	void OnButtonReleasedForgotPassword();

	UFUNCTION()
	void OnTextBlockCommittedLogin(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnSaveEmailStateChanged(bool bIsChecked);

	UFUNCTION()
	void OnSaveCredentialsStateChanged(bool bIsChecked);

	UFUNCTION()
	void Login();
};
