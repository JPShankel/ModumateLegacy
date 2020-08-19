// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "Components/WidgetSwitcher.h"


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

void USelectionTrayWidget::OpenToolTrayForSwap(EToolMode ToolMode, const FName &PresetToSwap)
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
}
