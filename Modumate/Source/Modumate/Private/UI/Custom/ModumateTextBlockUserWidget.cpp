// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom//ModumateTextBlock.h"
#include "UI/ModumateUIStatics.h"
#include "UnrealClasses/TooltipManager.h"

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

	if (ModumateTextBlock)
	{
		ModumateTextBlock->OverrideTextWrap(AutoWrapTextOverride, WrapTextAtOverride, WrappingPolicyOverride);
	}
	ToolTipWidgetDelegate.BindDynamic(this, &UModumateTextBlockUserWidget::OnTooltipWidget);
	return true;
}

void UModumateTextBlockUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateTextBlockUserWidget::ChangeText(const FText &NewText, bool EllipsizeText)
{
	if (!ModumateTextBlock)
	{
		return;
	}

	if (EllipsizeText)
	{
		FString textEllipsized = UModumateUIStatics::GetEllipsizeString(NewText.ToString(), EllipsizeWordAt);
		ModumateTextBlock->SetText(FText::FromString(textEllipsized)); // Potential loss of auto localization
	}
	else
	{
		ModumateTextBlock->SetText(NewText);
	}

}

UWidget* UModumateTextBlockUserWidget::OnTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID, this);
}
