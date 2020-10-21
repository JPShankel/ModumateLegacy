// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockModes.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Components/WrapBox.h"

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

void UToolTrayBlockModes::ChangeToMetaPlaneToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = { ButtonMetaPlaneLine, ButtonMetaPlaneHorizontal, ButtonMetaPlaneVertical };
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockModes::ChangeToSurfaceGraphToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = { ButtonAxesNone };
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockModes::ChangeToSeparatorToolsButtons(EToolMode mode)
{
	TArray<UModumateButtonUserWidget*> buttonsToShow;

	switch (mode)
	{
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_ROOF_PERIMETER:
		buttonsToShow.Append({ ButtonAxesNone, ButtonAxesXY, ButtonAxesZ, ButtonMPBucket, ButtonRoofPerimeter });
		break;

	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
		buttonsToShow.Append({ ButtonMPBucket, ButtonOpeningStamp });
		break;

	default:
		buttonsToShow.Append({ ButtonAxesNone, ButtonAxesXY, ButtonAxesZ, ButtonMPBucket });
		break;
	}

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
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!controller)
	{
		return;
	}

	UEditModelToolBase* currentTool = Cast<UEditModelToolBase>(controller->CurrentTool.GetObject());
	if (!currentTool)
	{
		return;
	}

	EAxisConstraint currentConstraint = currentTool->GetAxisConstraint();
	EToolCreateObjectMode currentDrawMode = currentTool->GetCreateObjectMode();

	if (controller->GetToolMode() == EToolMode::VE_METAPLANE)
	{
		switch (currentConstraint)
		{
		case EAxisConstraint::None:
			ButtonMetaPlaneLine->SwitchToActiveStyle();
			break;
		case EAxisConstraint::AxisZ:
			ButtonMetaPlaneHorizontal->SwitchToActiveStyle();
			break;
		case EAxisConstraint::AxesXY:
			ButtonMetaPlaneVertical->SwitchToActiveStyle();
			break;
		}
	}
	else if (currentDrawMode == EToolCreateObjectMode::Apply)
	{
		ButtonMPBucket->SwitchToActiveStyle();
	}
	else if (currentDrawMode == EToolCreateObjectMode::Stamp)
	{
		ButtonOpeningStamp->SwitchToActiveStyle();
	}
	else
	{
		switch (currentConstraint)
		{
		case EAxisConstraint::None:
			ButtonAxesNone->SwitchToActiveStyle();
			break;
		case EAxisConstraint::AxisZ:
			ButtonAxesXY->SwitchToActiveStyle();
			break;
		case EAxisConstraint::AxesXY:
			ButtonAxesZ->SwitchToActiveStyle();
			break;
		}
	}
}