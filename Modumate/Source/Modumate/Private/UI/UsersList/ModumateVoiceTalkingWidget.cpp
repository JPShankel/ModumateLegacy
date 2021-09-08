// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/UsersList/ModumateVoiceTalkingWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/ModumateGameInstance.h"


UModumateVoiceTalkingWidget::UModumateVoiceTalkingWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateVoiceTalkingWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	SetVisibility(ESlateVisibility::Collapsed);

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!controller)
	{
		return false;
	}

	if (!controller->VoiceClient)
	{
		controller->OnVoiceClientConnected().AddUObject(this, &UModumateVoiceTalkingWidget::VoiceClientConnectedHandler);
	}
	else
	{
		VoiceClientConnectedHandler();
	}

	return true;
}

void UModumateVoiceTalkingWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateVoiceTalkingWidget::TalkingChangedHandler(FString SpeakingUser, bool IsSpeaking)
{
	bool newUserSet = false;
	if (IsSpeaking)
	{
		newUserSet = SetToUser(SpeakingUser);
		if (newUserSet)
		{
			SpeakerMap.Add(SpeakingUser, IsSpeaking);
			SetVisibility(ESlateVisibility::Visible);
		}
	}

	if(!newUserSet)
	{
		SpeakerMap.Remove(SpeakingUser);
		if (SpeakerMap.Num() == 0)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (LatestSpeaker.Equals(SpeakingUser))
		{
			//The latest speaker has stopped talking, oh no!
			// Just choose a new one from the map.
			TArray<FString> keys;
			bool found = false;
			SpeakerMap.GetKeys(keys);
			for (int i = 0; i < keys.Num(); i++)
			{
				LatestSpeaker = keys[i];
				found = SetToUser(LatestSpeaker);
				if (found)
				{
					break;
				}
			}

			//This is a catch all -- if we don't know anything about the speakers
			// just don't display...
			if (!found)
			{
				SpeakerMap.Empty();
				SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UModumateVoiceTalkingWidget::ParticipantLeaveHandler(FString Participant)
{
	TalkingChangedHandler(Participant, false);
}

void UModumateVoiceTalkingWidget::VoiceClientConnectedHandler()
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!controller)
	{
		return;
	}

	controller->VoiceClient->OnTalkingChanged().AddUObject(this, &UModumateVoiceTalkingWidget::TalkingChangedHandler);
	controller->VoiceClient->OnParticipantLeave().AddUObject(this, &UModumateVoiceTalkingWidget::ParticipantLeaveHandler);
}

bool UModumateVoiceTalkingWidget::SetToUser(FString SpeakingUser)
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	AEditModelGameState* gameState = GetWorld()->GetGameState<AEditModelGameState>();

	if (!controller || !gameInstance || !gameState)
	{
		return false;
	}

	auto speakingPlayerState = gameState->GetPlayerFromCloudID(SpeakingUser);
	if (speakingPlayerState != nullptr)
	{
		UserListButton->SetupFromPlayerState(speakingPlayerState);
		auto text = FText::FromString(speakingPlayerState->ReplicatedUserInfo.Firstname);
		UserName->SetText(text);

		LatestSpeaker = SpeakingUser;
		return true;
	}

	return false;
}
