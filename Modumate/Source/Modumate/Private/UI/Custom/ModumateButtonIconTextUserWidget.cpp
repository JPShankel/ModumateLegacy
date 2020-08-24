// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "Components/Image.h"
#include "UI/Custom//ModumateTextBlock.h"

UModumateButtonIconTextUserWidget::UModumateButtonIconTextUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateButtonIconTextUserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ModumateButton && ButtonText && ButtonIcon))
	{
		return false;
	}

	ButtonText->ModumateTextBlock->OverrideTextWrap(ButtonTextAutoWrapTextOverride, ButtonTextWrapTextAtOverride, ButtonTextWrappingPolicyOverride);
	ButtonText->EllipsizeWordAt = ButtonTextEllipsizeWordAt;
	ButtonText->ChangeText(ButtonTextOverride);
	if (ButtonImageOverride)
	{
		ButtonIcon->SetBrushFromTexture(ButtonImageOverride);
	}
	return true;
}

void UModumateButtonIconTextUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}
