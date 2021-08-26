// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Chat/ModumateChatWidget.h"
#include "UI/Chat/ModumateChatMessageWidget.h"
#include "UI/Custom/ModumateSelectableTextBlock.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ModumateUIStatics.h"

#include "Online/ModumateAccountManager.h"

#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"

UModumateChatWidget::UModumateChatWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateChatWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	if (!Controller)
	{
		return false;
	}

	if (!Controller->TextChatClient)
	{
		Controller->OnTextChatClientConnected().AddUObject(this, &UModumateChatWidget::OnTextChatConnected);
	}
	else
	{
		OnTextChatConnected();
	}

	Button_Close->ModumateButton->OnReleased.AddDynamic(this, &UModumateChatWidget::OnButtonReleaseClose);
	ListView_ChatHistory->OnEntryWidgetGenerated().AddUObject(this, &UModumateChatWidget::ListEntryGeneratedHandler);

	return true;
}

void UModumateChatWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateChatWidget::NextTextChatHandler(const FString& FromID, const FString& Msg)
{
	AEditModelGameState* gameState = GetWorld()->GetGameState<AEditModelGameState>();
	if (gameState == nullptr)
	{
		return;
	}

	auto entryPlayerState = gameState->GetPlayerFromCloudID(FromID);
	if (entryPlayerState == nullptr)
	{
		return;
	}

	UChatMessage* newEntry = Cast<UChatMessage>(NewObject<UChatMessage>());
	if (newEntry == nullptr)
	{
		return;
	}
	
	UE_LOG(ModumateTextChat, Log, TEXT("Received Text from %s: %s"), *FromID, *Msg);

	newEntry->ID = FromID;
	int32 year, month, dayOfWeek, day, hour, minute, second, millisecond;
	FPlatformTime::SystemTime(year, month, dayOfWeek, day, hour, minute, second, millisecond);
	FString ampm = "AM";
	if (hour >= 12)
	{
		ampm = "PM";
		hour = hour - 12;
	}
	newEntry->Timestamp = FString::Printf(TEXT("%02d:%02d %s"), hour, minute, *ampm);
	newEntry->Message = Msg;
	newEntry->UserIconColor = entryPlayerState->GetClientColor();
	newEntry->Firstname = entryPlayerState->ReplicatedUserInfo.Firstname;
	newEntry->Lastname = entryPlayerState->ReplicatedUserInfo.Lastname;

	if (ListView_ChatHistory)
	{
		ListView_ChatHistory->AddItem(newEntry);
		ListView_ChatHistory->ScrollToBottom();
		ListView_ChatHistory->RequestRefresh();
		Text_EmptyChat->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UModumateChatWidget::OnButtonReleaseClose()
{
	if (Controller)
	{
		Controller->EditModelUserWidget->ToggleTextChat(false);
	}
}

void UModumateChatWidget::OnTextChatConnected()
{
	if (Controller->TextChatClient)
	{
		Controller->TextChatClient->OnNewTextChat().AddUObject(this, &UModumateChatWidget::NextTextChatHandler);
	}
}

void UModumateChatWidget::ListEntryGeneratedHandler(UUserWidget& NewEntry)
{
	auto* entry = Cast<UModumateChatMessageWidget>(&NewEntry);
	if (entry)
	{
		auto chatMessage = Cast<UChatMessage>(entry->GetListItem());

		FString username = chatMessage->Firstname + TEXT(" ") + chatMessage->Lastname;

		entry->UserIcon->Setup(chatMessage->Firstname, chatMessage->Lastname, chatMessage->UserIconColor);
		entry->Username->ChangeText(FText::FromString(username));
		entry->Timestamp->ChangeText(FText::FromString(chatMessage->Timestamp));
		entry->MessageTextSelectable->SetText(FText::FromString(chatMessage->Message));

		entry->SetVisibility(ESlateVisibility::Visible);
		entry->MessageTextSelectable->SetVisibility(ESlateVisibility::Visible);
	}
}
 