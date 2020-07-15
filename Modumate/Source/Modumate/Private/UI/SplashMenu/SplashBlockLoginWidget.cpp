// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashBlockLoginWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "Components/TextBlock.h"

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
	ModumateGameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	if (!(ButtonCreateAccount && ButtonLogin))
	{
		return false;
	}
	ButtonCreateAccount->ModumateButton->OnReleased.AddDynamic(this, &USplashBlockLoginWidget::OnButtonReleasedCreateAccount);
	ButtonLogin->ModumateButton->OnReleased.AddDynamic(this, &USplashBlockLoginWidget::Login);
	EmailBox->OnTextCommitted.AddDynamic(this, &USplashBlockLoginWidget::OnTextBlockCommittedLogin);
	PasswordBox->OnTextCommitted.AddDynamic(this, &USplashBlockLoginWidget::OnTextBlockCommittedLogin);

	return true;
}

void USplashBlockLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USplashBlockLoginWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Set LoginMessage visibility based on login status
	if (ModumateGameInstance && TextBlock_LoginAttemptMsg)
	{
		switch (ModumateGameInstance->LoginStatus())
		{
		case ELoginStatus::InvalidEmail:
		case ELoginStatus::InvalidPassword:
			TextBlock_LoginAttemptMsg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			break;
		default:
			TextBlock_LoginAttemptMsg->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USplashBlockLoginWidget::OnButtonReleasedCreateAccount()
{
	FPlatformProcess::LaunchURL(*CreateAccountURL, nullptr, nullptr);
}

void USplashBlockLoginWidget::OnTextBlockCommittedLogin(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Login();
	}
}

void USplashBlockLoginWidget::Login()
{
	if (ModumateGameInstance && EmailBox && PasswordBox)
	{
		ModumateGameInstance->Login(EmailBox->GetText().ToString(), PasswordBox->GetText().ToString());
	}
}
