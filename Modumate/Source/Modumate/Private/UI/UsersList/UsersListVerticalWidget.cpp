// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/UsersList/UsersListVerticalWidget.h"
#include "UI/UsersList/UsersListVerticalEntryWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerState.h"

UUsersListVerticalWidget::UUsersListVerticalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UUsersListVerticalWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!UserList_ListView)
	{
		return false;
	}

	UserList_ListView->OnEntryWidgetGenerated().AddUObject(this, &UUsersListVerticalWidget::ListEntryGeneratedHandler);

	return true;
}

bool UUsersListVerticalWidget::Clear()
{
	UserList_ListView->ClearListItems();
	return true;
}

bool UUsersListVerticalWidget::AddUser(AEditModelPlayerState* UserPlayerState)
{
	UUserVerticalEntry* newEntry = Cast<UUserVerticalEntry>(NewObject<UUserVerticalEntry>());
	if (newEntry == nullptr)
	{
		return false;
	}

	newEntry->UserIconColor = UserPlayerState->GetClientColor();
	newEntry->Firstname = UserPlayerState->ReplicatedUserInfo.Firstname;
	newEntry->Lastname = UserPlayerState->ReplicatedUserInfo.Lastname;
	newEntry->Email = UserPlayerState->ReplicatedUserInfo.Email;

	if (UserList_ListView)
	{
		UserList_ListView->AddItem(newEntry);
		UserList_ListView->ScrollToBottom();
		UserList_ListView->RequestRefresh();
	}

	return true;
}

void UUsersListVerticalWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UUsersListVerticalWidget::ListEntryGeneratedHandler(UUserWidget& NewEntry)
{
	auto* entry = Cast<UUsersListVerticalEntryWidget>(&NewEntry);
	if (entry)
	{
		auto user = Cast<UUserVerticalEntry>(entry->GetListItem());
		
		FString username = user->Firstname + TEXT(" ") + user->Lastname;

		entry->UserIcon->Setup(user->Firstname, user->Lastname, user->UserIconColor);
		entry->Username->ChangeText(FText::FromString(username));
		entry->Email->ChangeText(FText::FromString(user->Email));

		entry->SetVisibility(ESlateVisibility::Visible);
	}
}
