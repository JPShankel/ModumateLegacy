// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Chat/ModumateChatEditableTextWidget.h"

#include "UI/ModumateUIStatics.h"
#include "UI/EditModelUserWidget.h"
#include "Online/ModumateAccountManager.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"


UModumateChatEditableTextWidget::UModumateChatEditableTextWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateChatEditableTextWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	if (!Controller || !Checkbox_VoiceMicToggle)
	{
		return false;
	}

	Checkbox_VoiceMicToggle->OnCheckStateChanged.AddDynamic(this, &UModumateChatEditableTextWidget::OnMicToggled);
	Button_Submit->OnReleased.AddDynamic(this, &UModumateChatEditableTextWidget::OnSubmitReleased);
	TextBox_TextEntry->OnTextChanged.AddDynamic(this, &UModumateChatEditableTextWidget::OnTextChanged);

	return true;
}

void UModumateChatEditableTextWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateChatEditableTextWidget::OnMicToggled(bool bIsChecked)
{
	if (Controller && Controller->VoiceClient)
	{
		Controller->VoiceClient->SetMuted(!bIsChecked);
	}
}

void UModumateChatEditableTextWidget::OnSubmitReleased()
{
	SubmitTextToChat();
}

void UModumateChatEditableTextWidget::SubmitTextToChat()
{
	if (Controller && Controller->TextChatClient && TextBox_TextEntry)
	{
		FText text = TextBox_TextEntry->GetText();
		FString msg = text.ToString();
		msg = msg.TrimStartAndEnd();
		if (!msg.IsEmpty())
		{
			TextBox_TextEntry->SetText(FText::FromString(TEXT("")));
			Controller->TextChatClient->SendText(msg);
		}
	}
}

void UModumateChatEditableTextWidget::OnTextChanged(const FText& InText)
{
	FString currentString = InText.ToString();
	if (Controller  &&currentString.EndsWith(TEXT("\n")))
	{
		bool shouldPreventSending = Controller->IsInputKeyDown(EKeys::LeftShift) || Controller->IsInputKeyDown(EKeys::RightShift);

		if (!shouldPreventSending)
		{
			SubmitTextToChat();
		}
	}
}

