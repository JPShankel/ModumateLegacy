// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/EditModelUserWidget.h"
#include "Components/ListView.h"
#include "UI/Chat/ModumateChatMessageWidget.h"
#include "ModumateChatWidget.generated.h"

UCLASS()
class MODUMATE_API UChatMessage : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString ID;

	UPROPERTY()
	FString Timestamp;
	UPROPERTY()
	FString Message;

	UPROPERTY()
	FString Firstname;
	UPROPERTY()
	FString Lastname;
	UPROPERTY()
	FLinearColor UserIconColor;

	UChatMessage() :
		ID(TEXT("")),
		Timestamp(TEXT("")),
		Message(TEXT("")),
		Firstname(TEXT("")),
		Lastname(TEXT("")),
		UserIconColor(FLinearColor(0,0,0))
	{
	}
};


UCLASS()
class MODUMATE_API UModumateChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateChatWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY()
	class AEditModelPlayerController* Controller = nullptr;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_Close;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateChatEditableTextWidget* ChatEntryBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* ListView_ChatHistory;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Text_EmptyChat;

	UFUNCTION()
	void NextTextChatHandler(const FString& FromID, const FString& Msg);

	UFUNCTION()
	void OnButtonReleaseClose();

	UFUNCTION()
	void OnTextChatConnected();

	void ListEntryGeneratedHandler(UUserWidget& NewEntry);
};
