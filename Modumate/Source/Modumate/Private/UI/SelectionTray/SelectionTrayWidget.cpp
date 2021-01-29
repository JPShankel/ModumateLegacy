// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayWidget.h"

#include "Components/WidgetSwitcher.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/EditModelUserWidget.h"
#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/EditModelPlayerController.h"

USelectionTrayWidget::USelectionTrayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USelectionTrayWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void USelectionTrayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// TODO: take this out once the properties block is meaningful
	ToolTrayBlockProperties->SetVisibility(ESlateVisibility::Collapsed);
}

void USelectionTrayWidget::OpenToolTrayForSelection()
{
	// Switcher switches between which widget to display, depending on child order
	// SelectionTray is in first index, SwapTray is second
	WidgetSwitcherTray->SetActiveWidgetIndex(0);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionTrayBlockPresetList->BuildPresetListFromSelection();

	// TODO: Set menu animation here
}

void USelectionTrayWidget::OpenToolTrayForSwap(EToolMode ToolMode, const FGuid& PresetToSwap)
{
	// Switcher switches between which widget to display, depending on child order
	// SelectionTray is in first index, SwapTray is second
	WidgetSwitcherTray->SetActiveWidgetIndex(1);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CurrentPresetToSwap = PresetToSwap;
	SelectionTray_Block_Swap->CreatePresetListInAssembliesListForSwap(ToolMode, PresetToSwap);

	// TODO: Set menu animation here
}

void USelectionTrayWidget::CloseToolTray()
{
	// TODO: Set menu animation here
	SetVisibility(ESlateVisibility::Collapsed);
	SelectionTrayBlockPresetList->ClearPresetList();

	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget->BIMDesigner->GetVisibility() != ESlateVisibility::Collapsed)
	{
		controller->EditModelUserWidget->ToggleBIMDesigner(false);
	}
}
