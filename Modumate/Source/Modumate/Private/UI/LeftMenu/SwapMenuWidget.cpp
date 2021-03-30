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

void USwapMenuWidget::SetSwapMenuAsFromNode(const FGuid& InParentPresetGUID, const FGuid& InPresetGUIDToSwap, const FBIMEditorNodeIDType& InNodeID, const FBIMPresetFormElement& InFormElement)
{
	PresetGUIDToSwap = InPresetGUIDToSwap;
	ParentPresetGUIDToSwap = InParentPresetGUID;
	BIMNodeIDToSwap = InNodeID;
	BIMPresetFormElementToSwap = InFormElement;
	CurrentSwapMenuType = ESwapMenuType::SwapFromNode;
}

void USwapMenuWidget::BuildSwapMenu()
{
	NCPNavigatorWidget->BuildNCPNavigator(EPresetCardType::Swap);
}
