// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/SwapMenuWidget.h"

#include "UI/LeftMenu/NCPNavigator.h"
#include "UnrealClasses/EditModelPlayerController.h"

USwapMenuWidget::USwapMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USwapMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void USwapMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void USwapMenuWidget::SetSwapMenuAsSelection(FGuid InPresetGUIDToSwap)
{
	PresetGUIDToSwap = InPresetGUIDToSwap;
	CurrentSwapMenuType = ESwapMenuType::SwapFromSelection;
}

void USwapMenuWidget::BuildSwapMenu()
{
	NCPNavigatorWidget->BuildNCPNavigator(EPresetCardType::Swap);
}
