// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/UsersList/UsersListVerticalEntryWidget.h"

UUsersListVerticalEntryWidget::UUsersListVerticalEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UUsersListVerticalEntryWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UUsersListVerticalEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
}