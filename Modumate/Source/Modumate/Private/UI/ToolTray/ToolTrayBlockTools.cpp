// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockTools.h"
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

void UToolTrayBlockTools::ChangeToSeparatorToolsButtons()
{
	Button_Wall->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Floor->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Ceiling->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Roof->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Stair->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Door->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Window->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_SystemPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Mullion->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_BeamColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Railing->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Finish->SetVisibility(ESlateVisibility::Collapsed);
	Button_Trim->SetVisibility(ESlateVisibility::Collapsed);
	Button_Cabinet->SetVisibility(ESlateVisibility::Collapsed);
	Button_Countertop->SetVisibility(ESlateVisibility::Collapsed);
	Button_FFE->SetVisibility(ESlateVisibility::Collapsed);
}

void UToolTrayBlockTools::ChangeToAttachmentToolsButtons()
{
	Button_Wall->SetVisibility(ESlateVisibility::Collapsed);
	Button_Floor->SetVisibility(ESlateVisibility::Collapsed);
	Button_Ceiling->SetVisibility(ESlateVisibility::Collapsed);
	Button_Roof->SetVisibility(ESlateVisibility::Collapsed);
	Button_Stair->SetVisibility(ESlateVisibility::Collapsed);
	Button_Door->SetVisibility(ESlateVisibility::Collapsed);
	Button_Window->SetVisibility(ESlateVisibility::Collapsed);
	Button_SystemPanel->SetVisibility(ESlateVisibility::Collapsed);
	Button_Mullion->SetVisibility(ESlateVisibility::Collapsed);
	Button_BeamColumn->SetVisibility(ESlateVisibility::Collapsed);
	Button_Railing->SetVisibility(ESlateVisibility::Collapsed);
	Button_Finish->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Trim->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Cabinet->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_Countertop->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_FFE->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UToolTrayBlockTools::NativeConstruct()
{
	Super::NativeConstruct();
}