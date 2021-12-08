// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockTools.h"

#include "Components/WrapBox.h"
#include "UI/Custom/ModumateButtonUserWidget.h"

UToolTrayBlockTools::UToolTrayBlockTools(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolTrayBlockTools::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UToolTrayBlockTools::ChangeToMassingGraphToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = { Button_Line, Button_Rectangle };
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockTools::ChangeToSeparatorToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = { 
		Button_Wall, 
		Button_Floor, 
		Button_Ceiling, 
		Button_Roof, 
		Button_Stair, 
		Button_Door, 
		Button_Window, 
		Button_SystemPanel, 
		Button_Mullion, 
		Button_BeamColumn, 
		Button_Railing, 
		Button_Countertop,
		Button_PointHosted,
		Button_EdgeHosted
	};
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockTools::ChangeToAttachmentToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = { 
		Button_Finish, 
		Button_Trim, 
		Button_Cabinet, 
		Button_FFE
	};
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockTools::ChangeToSurfaceGraphToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = { Button_SurfaceLine };
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockTools::ChangeToSiteToolsButtons()
{
	TArray<UModumateButtonUserWidget*> buttonsToShow = { Button_TerrainGraph };
	SetButtonsState(buttonsToShow);
}

void UToolTrayBlockTools::NativeConstruct()
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

void UToolTrayBlockTools::SetButtonsState(const TArray<UModumateButtonUserWidget*>& ButtonsToShow)
{
	for (auto curButton : AllModumateButtons)
	{
		curButton->SetVisibility(ButtonsToShow.Contains(curButton) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		curButton->SwitchToNormalStyle();
	}
}
