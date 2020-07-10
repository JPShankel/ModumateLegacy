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
	ButtonAxesNone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonAxesXY->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonAxesZ->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPBucket->SetVisibility(ESlateVisibility::Collapsed);
	ButtonRoofPerimeter->SetVisibility(ESlateVisibility::Collapsed);
}

void UToolTrayBlockModes::ChangeToSurfaceGraphToolsButtons()
{
	ButtonAxesNone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonAxesXY->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonAxesZ->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPBucket->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonRoofPerimeter->SetVisibility(ESlateVisibility::Collapsed);
}

void UToolTrayBlockModes::ChangeToSeparatorToolsButtons(EToolMode mode)
{
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
