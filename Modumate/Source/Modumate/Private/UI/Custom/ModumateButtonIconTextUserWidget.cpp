// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "Components/Image.h"
#include "UI/Custom//ModumateTextBlock.h"
#include "UnrealClasses/TooltipManager.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"

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

	ModumateButton->OnReleased.AddDynamic(this, &UModumateButtonIconTextUserWidget::OnButtonPress);
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

void UModumateButtonIconTextUserWidget::OnButtonPress()
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && InputCommand != EInputCommand::None)
	{
		controller->InputHandlerComponent->TryCommand(InputCommand);
	}
}

UWidget* UModumateButtonIconTextUserWidget::OnTooltipWidget()
{
	if (!TooltipID.IsNone())
	{
		return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID, this);
	}
	else if (InputCommand != EInputCommand::None)
	{
		return UTooltipManager::GenerateTooltipWithInputWidget(InputCommand, this);
	}

	return nullptr;
}
