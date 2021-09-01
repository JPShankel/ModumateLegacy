// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/UsersList/UsersListHorizontalWidget.h"
#include "UI/UsersList/UsersListVerticalWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Components/SizeBox.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/UsersList/UsersListButtonWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"

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
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (ensure(controller))
	{
		bool isVisible = controller->EditModelUserWidget->UsersListVertical->Visibility != ESlateVisibility::Collapsed;
		controller->EditModelUserWidget->ToggleVerticalUserList(!isVisible);
	}
}

void UUsersListHorizontalWidget::UpdateHorizontalUsersList(const TArray<AEditModelPlayerState*>& InPlayerStates)
{
	// Set user buttons based on available player states
	const TArray<UUsersListButtonWidget*> userButtons = GetUserListButtonsArray();
	for (int32 i = 0; i < userButtons.Num(); ++i)
	{
		if (InPlayerStates.Num() > i)
		{
			userButtons[i]->SetVisibility(ESlateVisibility::Visible);
			userButtons[i]->SetupFromPlayerState(InPlayerStates[i]);

			//Set tooltip to the User's Fullname
			FString fullname = InPlayerStates[i]->ReplicatedUserInfo.Firstname + TEXT(" ") + InPlayerStates[i]->ReplicatedUserInfo.Lastname;
			FString completeTooltipText = fullname + TEXT("\n") + InPlayerStates[i]->ReplicatedUserInfo.Email;
			UTooltipWidget* tooltip = CreateWidget<UTooltipWidget>(GetWorld()->GetGameInstance(), UsersListTooltipClass);
			tooltip->TextBlock_Desc->TextOverride = FText::FromString(TEXT(""));
			tooltip->TextBlock_Desc->ModumateTextBlock->Text = FText::FromString(completeTooltipText);
			userButtons[i]->SetToolTip(tooltip);
		}
		else
		{
			userButtons[i]->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// Visibility of the expand button
	if (userButtons.Num() >= InPlayerStates.Num())
	{
		SizeBox_ExpandListButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		SizeBox_ExpandListButton->SetVisibility(ESlateVisibility::Visible);
		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		if (controller)
		{
			controller->EditModelUserWidget->UsersListVertical->Clear();
			for (int32 i = userButtons.Num(); i < InPlayerStates.Num(); i++)
			{
				auto ps = InPlayerStates[i];
				if (ps)
				{
					controller->EditModelUserWidget->UsersListVertical->AddUser(InPlayerStates[i]);
				}
			}
		}
	}
}

TArray<class UUsersListButtonWidget*> UUsersListHorizontalWidget::GetUserListButtonsArray() const
{
	return TArray<UUsersListButtonWidget*> {
		UsersListRoundButtonWidget_0,
		UsersListRoundButtonWidget_1,
		UsersListRoundButtonWidget_2,
		UsersListRoundButtonWidget_3,
		UsersListRoundButtonWidget_4
	};
}
