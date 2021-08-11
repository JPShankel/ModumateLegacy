// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/UsersList/UsersListHorizontalWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Components/HorizontalBox.h"
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
	HorizontalBox_UsersList->ClearChildren();

	for (const auto& curPlayerState : InPlayerStates)
	{
		UUsersListButtonWidget* buttonWidget = CreateWidget<UUsersListButtonWidget>(this, UsersListButtonWidgetClass);
		if (buttonWidget)
		{
			buttonWidget->SetupFromPlayerState(curPlayerState);
			HorizontalBox_UsersList->AddChildToHorizontalBox(buttonWidget);
		}
	}

	// Visibility of the expand button
	ModumateButton_ExpandUsersList->SetVisibility(
		ExtraNumberOfPlayerStates > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// Text within the expand button
	FString extraCountString = FString(TEXT("+")) + FString::FromInt(ExtraNumberOfPlayerStates);
	TextBlock_NumberOfUsers->SetText(FText::FromString(extraCountString));
}
