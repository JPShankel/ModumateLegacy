// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockModes.h"
#include "UI/Custom/ModumateButtonUserWidget.h"

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
}

void UToolTrayBlockModes::ChangeToMetaPlaneToolsButtons()
{
	ButtonMPLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPVertical->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPHorizontal->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonAxesNone->SetVisibility(ESlateVisibility::Collapsed);
	ButtonAxesXY->SetVisibility(ESlateVisibility::Collapsed);
	ButtonAxesZ->SetVisibility(ESlateVisibility::Collapsed);
	ButtonMPBucket->SetVisibility(ESlateVisibility::Collapsed);
	ButtonRoofPerimeter->SetVisibility(ESlateVisibility::Collapsed);
}

void UToolTrayBlockModes::ChangeToSeparatorToolsButtons(EToolMode mode)
{
	ButtonMPLine->SetVisibility(ESlateVisibility::Collapsed);
	ButtonMPVertical->SetVisibility(ESlateVisibility::Collapsed);
	ButtonMPHorizontal->SetVisibility(ESlateVisibility::Collapsed);
	ButtonAxesNone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonAxesXY->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonAxesZ->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPBucket->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (mode == EToolMode::VE_ROOF_FACE || mode == EToolMode::VE_ROOF_PERIMETER)
	{
		ButtonRoofPerimeter->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		ButtonRoofPerimeter->SetVisibility(ESlateVisibility::Collapsed);
	}
}
