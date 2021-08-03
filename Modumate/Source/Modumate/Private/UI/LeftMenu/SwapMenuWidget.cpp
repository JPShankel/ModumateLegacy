// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/SwapMenuWidget.h"

#include "UI/LeftMenu/NCPNavigator.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Objects/ModumateObjectInstance.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/BIM/BIMDesigner.h"

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

bool USwapMenuWidget::IsCurrentObjsSelectionValidForSwap()
{
	if (CurrentSwapMenuType == ESwapMenuType::SwapFromNode)
	{
		return EMPlayerController->EditModelUserWidget->BIMDesigner->IsVisible();
	}

	// Swap is valid if any of the selected objects have the same preset as cached PresetGUIDToSwap
	for (const auto& curObj : EMPlayerController->EMPlayerState->SelectedObjects)
	{
		if (curObj->GetAssembly().PresetGUID == PresetGUIDToSwap)
		{
			return true;
		}
	}
	return false;
}

void USwapMenuWidget::OnReleaseButtonClose()
{
	EMPlayerController->EditModelUserWidget->SwitchLeftMenu(EMPlayerController->EditModelUserWidget->PreviousLeftMenuState);
}
