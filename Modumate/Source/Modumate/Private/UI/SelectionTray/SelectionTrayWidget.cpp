// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayWidget.h"

#include "Components/WidgetSwitcher.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MetaEdge.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/DetailDesigner/DetailContainer.h"
#include "UI/EditModelUserWidget.h"
#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

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

bool USelectionTrayWidget::GetDetailFromSelection(FGuid& OutDetailPreset, TSet<int32>& OutEdgeIDs)
{
	OutDetailPreset.Invalidate();
	OutEdgeIDs.Reset();

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	if (!(controller && controller->EMPlayerState && (controller->EMPlayerState->SelectedObjects.Num() > 0)))
	{
		return false;
	}

	for (auto selectedObj : controller->EMPlayerState->SelectedObjects)
	{
		auto selectedEdge = Cast<AMOIMetaEdge>(selectedObj);
		auto selectedEdgeDetail = selectedEdge ? selectedEdge->CachedEdgeDetailMOI : nullptr;
		FGuid selectedEdgeDetailID = selectedEdgeDetail ? selectedEdgeDetail->GetAssembly().UniqueKey() : FGuid();
		if (!selectedEdgeDetailID.IsValid())
		{
			OutDetailPreset.Invalidate();
			break;
		}

		if (!OutDetailPreset.IsValid())
		{
			OutDetailPreset = selectedEdgeDetailID;
		}
		else if (OutDetailPreset != selectedEdgeDetailID)
		{
			OutDetailPreset.Invalidate();
			break;
		}

		OutEdgeIDs.Add(selectedEdge->ID);
	}

	if (!OutDetailPreset.IsValid())
	{
		OutEdgeIDs.Reset();
	}

	return OutDetailPreset.IsValid();
}

void USelectionTrayWidget::OpenToolTrayForSelection()
{
	CloseDetailDesigner();

	// Switcher switches between which widget to display, depending on child order
	// SelectionTray is in first index, SwapTray is second, DetailDesigner is third
	WidgetSwitcherTray->SetActiveWidgetIndex(0);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionTrayBlockPresetList->BuildPresetListFromSelection();

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto currentTool = controller ? Cast<UEditModelToolBase>(controller->CurrentTool.GetObject()) : nullptr;
	if (currentTool && (currentTool->GetToolMode() == EToolMode::VE_SELECT))
	{
		ToolTrayBlockProperties->ChangeBlockProperties(currentTool);
	}
	else
	{
		ToolTrayBlockProperties->SetVisibility(ESlateVisibility::Collapsed);
	}

	// TODO: Set menu animation here
}

void USelectionTrayWidget::OpenToolTrayDetailDesigner(const FGuid& DetailPreset, const TSet<int32>& EdgeIDs)
{
	// Switcher switches between which widget to display, depending on child order
	// SelectionTray is in first index, SwapTray is second, DetailDesigner is third
	WidgetSwitcherTray->SetActiveWidgetIndex(2);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CurrentDetailPreset = DetailPreset;
	CurrentDetailEdgeIDs = EdgeIDs;
	if (!DetailDesigner->BuildEditor(CurrentDetailPreset, CurrentDetailEdgeIDs))
	{
		DetailDesigner->ClearEditor();
	}
}

void USelectionTrayWidget::UpdateFromSelection()
{
	// The current selection could either result in the detail designer or the selection viewer.
	// If the current selection shares the same objcts and detail that we were already designing, then update that;
	// otherwise, open the selection viewer.
	int32 numDetailsDesigning = CurrentDetailEdgeIDs.Num();
	FGuid sharedEdgeDetailID;
	TSet<int32> curSelectedEdgeIDs;
	if (GetDetailFromSelection(sharedEdgeDetailID, curSelectedEdgeIDs) &&
		(sharedEdgeDetailID == CurrentDetailPreset) &&
		(curSelectedEdgeIDs.Intersect(CurrentDetailEdgeIDs).Num() == numDetailsDesigning))
	{
		OpenToolTrayDetailDesigner(sharedEdgeDetailID, CurrentDetailEdgeIDs);
	}
	else
	{
		OpenToolTrayForSelection();
	}
}

void USelectionTrayWidget::StartDetailDesignerFromSelection()
{
	FGuid sharedEdgeDetailID;
	TSet<int32> edgeIDs;
	if (GetDetailFromSelection(sharedEdgeDetailID, edgeIDs))
	{
		OpenToolTrayDetailDesigner(sharedEdgeDetailID, edgeIDs);
	}
}

void USelectionTrayWidget::CloseDetailDesigner()
{
	CurrentDetailPreset.Invalidate();
	CurrentDetailEdgeIDs.Reset();
	DetailDesigner->ClearEditor();
}

void USelectionTrayWidget::CloseToolTray()
{
	// TODO: Set menu animation here
	SetVisibility(ESlateVisibility::Collapsed);
	SelectionTrayBlockPresetList->ClearPresetList();

	CloseDetailDesigner();
}
