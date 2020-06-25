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
	ButtonMPBucket->SetVisibility(ESlateVisibility::Collapsed);
	ButtonRoofPerimeter->SetVisibility(ESlateVisibility::Collapsed);
	ButtonOpeningStamp->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonOpeningSystem->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UToolTrayBlockModes::ChangeToSeparatorToolsButtons()
{
	ButtonMPLine->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPVertical->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPHorizontal->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonMPBucket->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ButtonRoofPerimeter->SetVisibility(ESlateVisibility::Collapsed);
	ButtonOpeningStamp->SetVisibility(ESlateVisibility::Collapsed);
	ButtonOpeningSystem->SetVisibility(ESlateVisibility::Collapsed);
}
