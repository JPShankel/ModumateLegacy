// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/TooltipManager.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "Components/Image.h"

UModumateButtonUserWidget::UModumateButtonUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateButtonUserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ModumateButton)
	{
		return false;
	}

	ModumateButton->OnReleased.AddDynamic(this, &UModumateButtonUserWidget::OnButtonPress);
	ToolTipWidgetDelegate.BindDynamic(this, &UModumateButtonUserWidget::OnTooltipWidget);

	if (ButtonText)
	{
		ButtonText->SetText(ButtonTextOverride);
	}

	if (ButtonImageOverride && ButtonImage)
	{
		ButtonImage->SetBrushFromTexture(ButtonImageOverride);
	}

	return true;
}

void UModumateButtonUserWidget::SetIsEnabled(bool bInIsEnabled)
{
	Super::SetIsEnabled(bInIsEnabled);

	OnEnabled(bInIsEnabled);
}

void UModumateButtonUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	NormalButtonStyle = ModumateButton->WidgetStyle;
	ActiveButtonStyle = ModumateButton->WidgetStyle;
	DisabledButtonStyle = ModumateButton->WidgetStyle;

	ActiveButtonStyle.Normal = NormalButtonStyle.Pressed;
	ActiveButtonStyle.Hovered = NormalButtonStyle.Pressed;

	DisabledButtonStyle.Normal = NormalButtonStyle.Disabled;
	DisabledButtonStyle.Hovered = NormalButtonStyle.Pressed;

	// Bind this button to its toolmode if available
	EToolMode inputToToolMode = UEditModelInputHandler::ToolModeFromInputCommand(InputCommand);
	if (inputToToolMode != EToolMode::VE_NONE)
	{		
		AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
		controller->EditModelUserWidget->ToolToButtonMap.Add(inputToToolMode, this);
	}
}

void UModumateButtonUserWidget::NativeDestruct()
{
	Super::NativeDestruct();

	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->ToolToButtonMap.Remove(UEditModelInputHandler::ToolModeFromInputCommand(InputCommand));
	}
}

void UModumateButtonUserWidget::OnButtonPress()
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && InputCommand != EInputCommand::None)
	{
		controller->InputHandlerComponent->TryCommand(InputCommand);
	}
}

void UModumateButtonUserWidget::SwitchToNormalStyle()
{
	ModumateButton->SetStyle(NormalButtonStyle);
}

void UModumateButtonUserWidget::SwitchToActiveStyle()
{
	ModumateButton->SetStyle(ActiveButtonStyle);
}

void UModumateButtonUserWidget::SwitchToDisabledStyle()
{
	ModumateButton->SetStyle(DisabledButtonStyle);
}

UWidget* UModumateButtonUserWidget::OnTooltipWidget()
{
	if (!TooltipID.IsNone())
	{
		return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID, this, OverrideTooltipText);
	}
	else if (InputCommand != EInputCommand::None)
	{
		return UTooltipManager::GenerateTooltipWithInputWidget(InputCommand, this, OverrideTooltipText);
	}

	return nullptr;
}
