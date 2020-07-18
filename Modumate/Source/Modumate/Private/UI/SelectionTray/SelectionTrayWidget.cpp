// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"


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
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionTrayBlockPresetList->BuildPresetListFromSelection();

	// TODO: Set menu animation here
}

void USelectionTrayWidget::OpenToolTrayForSwap()
{
	// TODO: Swap preset for actors
}

void USelectionTrayWidget::CloseToolTray()
{
	// TODO: Set menu animation here
	SetVisibility(ESlateVisibility::Collapsed);
	SelectionTrayBlockPresetList->ClearPresetList();
}
