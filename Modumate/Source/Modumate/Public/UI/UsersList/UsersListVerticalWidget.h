// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/UsersList/UsersListButtonWidget.h"
#include "UI/Custom/TooltipWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ListView.h"

#include "UsersListVerticalWidget.generated.h"

class AEditModelPlayerState;

UCLASS()
class MODUMATE_API UUserVerticalEntry : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Firstname;
	UPROPERTY()
	FString Lastname;
	UPROPERTY()
	FString Email;

	UPROPERTY()
	FLinearColor UserIconColor;

	UUserVerticalEntry() :
		Firstname(TEXT("")),
		Lastname(TEXT("")),
		Email(TEXT("")),
		UserIconColor(FLinearColor(0, 0, 0))
	{
	}
};

UCLASS()
class MODUMATE_API UUsersListVerticalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUsersListVerticalWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;
	virtual bool Clear();
	virtual bool AddUser(AEditModelPlayerState* UserPlayerState);

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UListView* UserList_ListView;

	void ListEntryGeneratedHandler(UUserWidget& NewEntry);
};
