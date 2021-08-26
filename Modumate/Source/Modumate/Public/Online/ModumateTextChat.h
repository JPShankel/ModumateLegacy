// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/ModumateCapability.h"
#include "ModumateTextChat.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ModumateTextChat, Log, All);


/// <summary>
/// AModumateTextChat is responsible for allowing users to chat with each
/// other while connected to a project.
/// </summary>
UCLASS()
class MODUMATE_API AModumateTextChat : public AModumateCapability
{
	GENERATED_BODY()

public:
	AModumateTextChat();
	virtual ~AModumateTextChat();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SendText(const FString& Msg);

	void Connect(const FString& Id);

	DECLARE_EVENT_TwoParams(AModumateTextChat, FTextChatEvent, const FString&, const FString&)
	FTextChatEvent& OnNewTextChat() { return NewTextChatEvent; }



private:
	UFUNCTION(Server, Reliable)
	void SERVER_SendText(const FString& FromID, const FString& Msg);

	UFUNCTION(Client, Reliable)
	void CLIENT_ReceiveText(const FString& FromID, const FString& Msg);

	FTextChatEvent NewTextChatEvent;
	FString OwnerID;
};
