// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/UsersList/UsersListButtonWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Online/ModumateAccountManager.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/ModumateUIStatics.h"


UUsersListButtonWidget::UUsersListButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UUsersListButtonWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UUsersListButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UUsersListButtonWidget::SetupFromPlayerState(const class AEditModelPlayerState* InOwnerPlayerState)
{
	if (InOwnerPlayerState && !InOwnerPlayerState->IsActorBeingDestroyed())
	{
		// Text string
		FString firstName = InOwnerPlayerState->ReplicatedUserInfo.Firstname;
		FString lastName = InOwnerPlayerState->ReplicatedUserInfo.Lastname;
		if (firstName.Len() > 0)
		{
			firstName = firstName.Left(1);
		}
		if (lastName.Len() > 0)
		{
			lastName = lastName.Left(1);
		}
		TextBlock_UserName->SetText(FText::FromString(firstName + lastName));

		// Text color
		FLinearColor textColor = UModumateUIStatics::GetTextColorFromBackgroundColor(InOwnerPlayerState->GetClientColor());
		TextBlock_UserName->SetColorAndOpacity(FSlateColor(textColor));

		// Background color
		ModumateButton->SetBackgroundColor(InOwnerPlayerState->GetClientColor());
	}
}
