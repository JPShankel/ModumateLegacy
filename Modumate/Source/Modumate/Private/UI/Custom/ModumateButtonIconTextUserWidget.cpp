// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "Components/Image.h"
#include "UI/Custom//ModumateTextBlock.h"
#include "UnrealClasses/TooltipManager.h"

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
	ToolTipWidgetDelegate.BindDynamic(this, &UModumateButtonIconTextUserWidget::OnTooltipWidget);
	return true;
}

void UModumateButtonIconTextUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

UWidget* UModumateButtonIconTextUserWidget::OnTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID, this);
}
