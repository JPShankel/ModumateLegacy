// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SplashBlockLoginWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API USplashBlockLoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USplashBlockLoginWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString CreateAccountURL;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonCreateAccount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonLogin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBox *EmailBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBox *PasswordBox;

	UFUNCTION()
	void OnButtonReleasedCreateAccount();

	UFUNCTION()
	void OnButtonReleasedLogin();
};
