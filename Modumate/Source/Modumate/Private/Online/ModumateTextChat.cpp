// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateTextChat.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(ModumateTextChat);


AModumateTextChat::AModumateTextChat() :
	AModumateCapability(),
	OwnerID(FString(TEXT("")))
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetUseOwnerRelevancy = false;
}


AModumateTextChat::~AModumateTextChat()
{
	UE_LOG(ModumateTextChat, Log, TEXT("ModumateTextChat Collected"));
}

void AModumateTextChat::BeginPlay()
{
	Super::BeginPlay();
}

void AModumateTextChat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AModumateTextChat::SendText(const FString& Msg)
{
	if (IsNetMode(NM_Client))
	{
		SERVER_SendText(OwnerID, Msg);
	}
}

void AModumateTextChat::Connect(const FString& Id)
{
	OwnerID = Id;
}

void AModumateTextChat::SERVER_SendText_Implementation(const FString& FromID, const FString& Msg)
{
	for (FConstPlayerControllerIterator idx = GetWorld()->GetPlayerControllerIterator(); idx; ++idx)
	{
		const auto* empc = Cast<AEditModelPlayerController>(idx->Get());
		if (empc && empc->TextChatClient)
		{
			empc->TextChatClient->CLIENT_ReceiveText(FromID, Msg);
		}
	}
}


void AModumateTextChat::CLIENT_ReceiveText_Implementation(const FString& FromID, const FString& Msg)
{
	NewTextChatEvent.Broadcast(FromID, Msg);
}
