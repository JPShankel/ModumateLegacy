// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/UsersList/UsersListHorizontalWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Components/SizeBox.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/UsersList/UsersListButtonWidget.h"
#include "UI/EditModelUserWidget.h"



UUsersListHorizontalWidget::UUsersListHorizontalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UUsersListHorizontalWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ModumateButton_ExpandUsersList)
	{
		return false;
	}

	ModumateButton_ExpandUsersList->OnReleased.AddDynamic(this, &UUsersListHorizontalWidget::OnReleaseButton_ExpandUsersList);

	return true;
}

void UUsersListHorizontalWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UUsersListHorizontalWidget::OnReleaseButton_ExpandUsersList()
{
	// TODO: Toggle expanded users list
}

void UUsersListHorizontalWidget::UpdateHorizontalUsersList(const TArray<AEditModelPlayerState*>& InPlayerStates, int32 ExtraNumberOfPlayerStates)
{
	// Set user buttons based on available player states
	const TArray<UUsersListButtonWidget*> userButtons = GetUserListButtonsArray();
	for (int32 i = 0; i < userButtons.Num(); ++i)
	{
		if (InPlayerStates.Num() > i)
		{
			userButtons[i]->SetVisibility(ESlateVisibility::Visible);
			userButtons[i]->SetupFromPlayerState(InPlayerStates[i]);
		}
		else
		{
			userButtons[i]->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// Visibility of the expand button
	SizeBox_ExpandListButton->SetVisibility(
		ExtraNumberOfPlayerStates > 0 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

	// Text within the expand button
	FString extraCountString = FString(TEXT("+")) + FString::FromInt(ExtraNumberOfPlayerStates);
	TextBlock_NumberOfUsers->SetText(FText::FromString(extraCountString));
}

TArray<class UUsersListButtonWidget*> UUsersListHorizontalWidget::GetUserListButtonsArray() const
{
	return TArray<UUsersListButtonWidget*> {
		UsersListRoundButtonWidget_0,
		UsersListRoundButtonWidget_1,
		UsersListRoundButtonWidget_2,
		UsersListRoundButtonWidget_3,
		UsersListRoundButtonWidget_4 };
}
