// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/TooltipManager.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"

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
	ActiveButtonStyle.Normal = NormalButtonStyle.Pressed;
	ActiveButtonStyle.Hovered = NormalButtonStyle.Pressed;

	// Bind this button to its toolmode if available
	EToolMode inputToToolMode = UEditModelInputHandler::ToolModeFromInputCommand(InputCommand);
	if (inputToToolMode != EToolMode::VE_NONE)
	{		
		AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		controller->EditModelUserWidget->ToolToButtonMap.Add(inputToToolMode, this);
	}
}

void UModumateButtonUserWidget::NativeDestruct()
{
	Super::NativeDestruct();

	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->ToolToButtonMap.Remove(UEditModelInputHandler::ToolModeFromInputCommand(InputCommand));
	}
}

void UModumateButtonUserWidget::OnButtonPress()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
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

UWidget* UModumateButtonUserWidget::OnTooltipWidget()
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
