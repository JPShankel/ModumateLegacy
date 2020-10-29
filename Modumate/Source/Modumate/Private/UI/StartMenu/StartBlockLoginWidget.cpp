// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartBlockLoginWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "Components/TextBlock.h"

UStartBlockLoginWidget::UStartBlockLoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStartBlockLoginWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	ModumateGameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	if (!(ButtonCreateAccount && ButtonLogin && ButtonForgotPassword))
	{
		return false;
	}
	ButtonCreateAccount->ModumateButton->OnReleased.AddDynamic(this, &UStartBlockLoginWidget::OnButtonReleasedCreateAccount);
	ButtonForgotPassword->ModumateButton->OnReleased.AddDynamic(this, &UStartBlockLoginWidget::OnButtonReleasedForgotPassword);
	ButtonLogin->ModumateButton->OnReleased.AddDynamic(this, &UStartBlockLoginWidget::Login);
	EmailBox->OnTextCommitted.AddDynamic(this, &UStartBlockLoginWidget::OnTextBlockCommittedLogin);
	PasswordBox->OnTextCommitted.AddDynamic(this, &UStartBlockLoginWidget::OnTextBlockCommittedLogin);

	return true;
}

void UStartBlockLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UStartBlockLoginWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void UStartBlockLoginWidget::OnButtonReleasedCreateAccount()
{
	FPlatformProcess::LaunchURL(*CreateAccountURL, nullptr, nullptr);
}

void UStartBlockLoginWidget::OnButtonReleasedForgotPassword()
{
	FPlatformProcess::LaunchURL(*ForgotPasswordURL, nullptr, nullptr);
}

void UStartBlockLoginWidget::OnTextBlockCommittedLogin(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Login();
	}
}

void UStartBlockLoginWidget::Login()
{
	if (ModumateGameInstance && EmailBox && PasswordBox)
	{
		ModumateGameInstance->Login(EmailBox->GetText().ToString(), PasswordBox->GetText().ToString());
	}
}
