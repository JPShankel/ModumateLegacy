// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashBlockLoginWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/Custom/ModumateEditableTextBox.h"

USplashBlockLoginWidget::USplashBlockLoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USplashBlockLoginWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!(ButtonCreateAccount && ButtonLogin))
	{
		return false;
	}

	ButtonCreateAccount->ModumateButton->OnReleased.AddDynamic(this, &USplashBlockLoginWidget::OnButtonReleasedCreateAccount);
	ButtonLogin->ModumateButton->OnReleased.AddDynamic(this, &USplashBlockLoginWidget::OnButtonReleasedLogin);

	return true;
}

void USplashBlockLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USplashBlockLoginWidget::OnButtonReleasedCreateAccount()
{
	FPlatformProcess::LaunchURL(*CreateAccountURL, nullptr, nullptr);
}

void USplashBlockLoginWidget::OnButtonReleasedLogin()
{
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	if (gameInstance && EmailBox && PasswordBox)
	{
		gameInstance->Login(EmailBox->GetText().ToString(), PasswordBox->GetText().ToString());
	}
}
