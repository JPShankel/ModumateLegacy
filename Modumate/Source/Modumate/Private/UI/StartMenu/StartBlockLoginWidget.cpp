// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartBlockLoginWidget.h"

#include "Components/TextBlock.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateCheckBox.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UnrealClasses/ModumateGameInstance.h"

#define LOCTEXT_NAMESPACE "ModumateUI"

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

	if (!(ButtonCreateAccount && ButtonLogin && ButtonForgotPassword &&
		EmailBox && PasswordBox && SaveEmail && SaveCredentials))
	{
		return false;
	}

	ButtonCreateAccount->ModumateButton->OnReleased.AddDynamic(this, &UStartBlockLoginWidget::OnButtonReleasedCreateAccount);
	ButtonForgotPassword->ModumateButton->OnReleased.AddDynamic(this, &UStartBlockLoginWidget::OnButtonReleasedForgotPassword);
	ButtonLogin->ModumateButton->OnReleased.AddDynamic(this, &UStartBlockLoginWidget::Login);
	EmailBox->OnTextCommitted.AddDynamic(this, &UStartBlockLoginWidget::OnTextBlockCommittedLogin);
	PasswordBox->OnTextCommitted.AddDynamic(this, &UStartBlockLoginWidget::OnTextBlockCommittedLogin);
	SaveEmail->OnCheckStateChanged.AddDynamic(this, &UStartBlockLoginWidget::OnSaveEmailStateChanged);
	SaveCredentials->OnCheckStateChanged.AddDynamic(this, &UStartBlockLoginWidget::OnSaveCredentialsStateChanged);

	if (ModumateGameInstance)
	{
		if (!ModumateGameInstance->UserSettings.SavedUserName.IsEmpty())
		{
			EmailBox->SetText(FText::FromString(ModumateGameInstance->UserSettings.SavedUserName));
			SaveEmail->SetIsChecked(true);
		}

		if (!ModumateGameInstance->UserSettings.SavedCredentials.IsEmpty())
		{
			PasswordBox->SetHintText(LOCTEXT("SavedPassword", "Using saved credentials"));
			SaveCredentials->SetIsChecked(true);
		}
	}

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
		case ELoginStatus::InvalidCredentials:
			TextBlock_LoginAttemptMsg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			break;
		default:
			TextBlock_LoginAttemptMsg->SetVisibility(ESlateVisibility::Collapsed);
			break;
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

void UStartBlockLoginWidget::OnSaveEmailStateChanged(bool bIsChecked)
{
	if (!bIsChecked)
	{
		SaveCredentials->SetIsChecked(false);
	}
}

void UStartBlockLoginWidget::OnSaveCredentialsStateChanged(bool bIsChecked)
{
	if (bIsChecked)
	{
		SaveEmail->SetIsChecked(true);
	}
}

void UStartBlockLoginWidget::Login()
{
	if (ModumateGameInstance && EmailBox && PasswordBox)
	{
		FText enteredUserName = EmailBox->GetText();
		FString userName = !enteredUserName.IsEmpty() ? enteredUserName.ToString() : ModumateGameInstance->UserSettings.SavedUserName;
		FString password = PasswordBox->GetText().ToString();
		FString refreshToken = password.IsEmpty() ? ModumateGameInstance->UserSettings.SavedCredentials : FString();
		bool bSaveUserName = SaveEmail->IsChecked();
		bool bSaveRefreshToken = SaveCredentials->IsChecked();

		ModumateGameInstance->Login(userName, password, refreshToken, bSaveUserName, bSaveRefreshToken);
	}
}

#undef LOCTEXT_NAMESPACE
