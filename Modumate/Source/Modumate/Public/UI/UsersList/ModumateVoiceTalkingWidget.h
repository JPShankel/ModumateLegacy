// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Online/ModumateAccountManager.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/ModumateUIStatics.h"
#include "UI/UsersList/UsersListButtonWidget.h"
#include "Containers/Map.h"

#include "ModumateVoiceTalkingWidget.generated.h"

UCLASS()
class MODUMATE_API UModumateVoiceTalkingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateVoiceTalkingWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* UserName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UserListButton;

protected:
	virtual void NativeConstruct() override;
	virtual void TalkingChangedHandler(FString SpeakingUser, bool IsSpeaking);
	virtual void ParticipantLeaveHandler(FString Participant);
	virtual void VoiceClientConnectedHandler();

	virtual bool SetToUser(FString SpeakingUser);

	TMap<FString, bool> SpeakerMap;
	FString LatestSpeaker;
	
};
