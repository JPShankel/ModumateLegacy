// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateDropDownUserWidget.h"

UModumateDropDownUserWidget::UModumateDropDownUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateDropDownUserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UModumateDropDownUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}