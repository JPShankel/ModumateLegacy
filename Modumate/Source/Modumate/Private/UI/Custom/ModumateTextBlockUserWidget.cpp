// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom//ModumateTextBlock.h"

UModumateTextBlockUserWidget::UModumateTextBlockUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateTextBlockUserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	ChangeText(TextOverride);
	return true;
}

void UModumateTextBlockUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateTextBlockUserWidget::ChangeText(FText NewText)
{
	ModumateTextBlock->SetText(NewText);
}
