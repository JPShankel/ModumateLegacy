// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockModes.h"

#include "Components/WrapBox.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"

UToolTrayBlockModes::UToolTrayBlockModes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolTrayBlockModes::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UToolTrayBlockModes::NativeConstruct()
{
	Super::NativeConstruct();

	for (const auto& curButton : WrapBox_Buttons->GetAllChildren())
	{
		UModumateButtonUserWidget* asModumateButton = Cast<UModumateButtonUserWidget>(curButton);
		if (asModumateButton)
		{
			AllModumateButtons.AddUnique(asModumateButton);
		}
	}
}

void UToolTrayBlockModes::ChangeToSeparatorToolsButtons(EToolMode mode)
{
	TArray<UModumateButtonUserWidget*> buttonsToShow;

	switch (mode)
	{
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_ROOF_PERIMETER:
		buttonsToShow.Append({ ButtonMPBucket, ButtonRectangle, ButtonRoofPerimeter });
		break;

	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
		buttonsToShow.Append({ ButtonMPBucket, ButtonOpeningStamp });
		break;

	case EToolMode::VE_MULLION:
	case EToolMode::VE_STRUCTURELINE:
		buttonsToShow.Append({ ButtonMPBucket, ButtonLine });
		break;

	default:
		buttonsToShow.Append({ ButtonMPBucket, ButtonRectangle });
		break;
	}

	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockModes::ChangeToAttachmentToolsButtons(EToolMode mode)
{
	SetButtonsState({ ButtonMPBucket });
}

void UToolTrayBlockModes::ChangeToSiteToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = {
		ButtonLine,
		ButtonPoint
	};
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockModes::SetButtonsState(const TArray<UModumateButtonUserWidget*>& ButtonsToShow)
{
	for (auto curButton : AllModumateButtons)
	{
		curButton->SetVisibility(ButtonsToShow.Contains(curButton) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		curButton->SwitchToNormalStyle();
	}

	// Set button active state based on current tool
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (!controller)
	{
		return;
	}

	UEditModelToolBase* currentTool = Cast<UEditModelToolBase>(controller->CurrentTool.GetObject());
	if (!currentTool)
	{
		return;
	}

	switch (currentTool->GetCreateObjectMode())
	{
	case EToolCreateObjectMode::Draw:
		ButtonRectangle->SwitchToActiveStyle();
		break;
	case EToolCreateObjectMode::Apply:
		ButtonMPBucket->SwitchToActiveStyle();
		break;
	case EToolCreateObjectMode::Stamp:
		ButtonOpeningStamp->SwitchToActiveStyle();
		break;
	}
}