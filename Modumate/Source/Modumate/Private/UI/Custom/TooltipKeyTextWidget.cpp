// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/TooltipKeyTextWidget.h"
#include "UI/Custom//ModumateTextBlock.h"

UTooltipKeyTextWidget::UTooltipKeyTextWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTooltipKeyTextWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	ChangeText(TextOverride);
	return true;
}

void UTooltipKeyTextWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UTooltipKeyTextWidget::ChangeText(const FText &NewText)
{
	if (!ModumateTextBlock)
	{
		return;
	}

	ModumateTextBlock->SetText(NewText);
}
