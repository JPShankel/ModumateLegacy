// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/SwapMenuWidget.h"

#include "UI/LeftMenu/NCPNavigator.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"

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

	if (!ButtonClose)
	{
		return false;
	}

	ButtonClose->ModumateButton->OnReleased.AddDynamic(this, &USwapMenuWidget::OnReleaseButtonClose);

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

void USwapMenuWidget::OnReleaseButtonClose()
{
	EMPlayerController->EditModelUserWidget->SwitchLeftMenu(EMPlayerController->EditModelUserWidget->PreviousLeftMenuState);
}
