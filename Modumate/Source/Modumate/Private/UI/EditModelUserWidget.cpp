// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/EditModelUserWidget.h"

#include "UI/BIM/BIMBlockDialogBox.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/ModalDialog/AlertAccountDialogWidget.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UI/RightMenu/ViewMenuBlockViewMode.h"
#include "UI/RightMenu/ViewMenuWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/Toolbar/ToolbarTopWidget.h"
#include "UI/Toolbar/ToolbarWidget.h"
#include "UI/Toolbar/ViewModeIndicatorWidget.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UI/Debugger/BIMDebugger.h"
#include "Components/Border.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "UI/BIM/BIMScopeWarning.h"

UEditModelUserWidget::UEditModelUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UEditModelUserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!(ToolTrayWidget && ToolTrayWidget))
	{
		return false;
	}

	if (Controller)
	{
		Controller->OnToolModeChanged.AddDynamic(this, &UEditModelUserWidget::UpdateOnToolModeChanged);
		Controller->OnToolCreateObjectModeChanged.AddDynamic(this, &UEditModelUserWidget::UpdateToolTray);
		Controller->OnToolAssemblyChanged.AddDynamic(this, &UEditModelUserWidget::UpdateToolTray);
	}

	ToolbarWidget->EditModelUserWidget = this;
	ToolTrayWidget->EditModelUserWidget = this;

	return true;
}

void UEditModelUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Update current menu visibility according to tool mode
	UpdateToolTray();
}

void UEditModelUserWidget::UpdateOnToolModeChanged()
{
	ToolTrayWidget->ToolTrayBlockAssembliesList->ResetSearchBox();
	UpdateToolTray();
}

void UEditModelUserWidget::UpdateToolTray()
{
	if (!(Controller && ToolTrayWidget))
	{
		return;
	}
	switch (UModumateTypeStatics::GetToolCategory(Controller->GetToolMode()))
	{
	case EToolCategories::MetaGraph:
		ToolTrayWidget->ChangeBlockToMetaPlaneTools();
		break;
	case EToolCategories::Separators:
		ToolTrayWidget->ChangeBlockToSeparatorTools(Controller->GetToolMode());
		break;
	case EToolCategories::SurfaceGraphs:
		ToolTrayWidget->ChangeBlockToSurfaceGraphTools();
		break;
	case EToolCategories::Attachments:
		ToolTrayWidget->ChangeBlockToAttachmentTools(Controller->GetToolMode());
		break;
	default:
		ToolTrayWidget->CloseToolTray();
		if (Controller->GetToolMode() == EToolMode::VE_CUTPLANE)
		{
			SwitchRightMenu(ERightMenuState::CutPlaneMenu);
		}

		// Close BIMDesigner if it's still open
		if (BIMDesigner->GetVisibility() != ESlateVisibility::Collapsed)
		{
			ToggleBIMDesigner(false);
		}
	}
	
	if (CurrentActiveToolButton && UEditModelInputHandler::ToolModeFromInputCommand(CurrentActiveToolButton->InputCommand) != Controller->GetToolMode())
	{
		CurrentActiveToolButton->SwitchToNormalStyle();
		CurrentActiveToolButton = nullptr;
	}
	UModumateButtonUserWidget* toolToButton = ToolToButtonMap.FindRef(Controller->GetToolMode());
	if (toolToButton)
	{
		CurrentActiveToolButton = toolToButton;
		CurrentActiveToolButton->SwitchToActiveStyle();
	}

	if (ViewMenu && ViewMenu->ViewMenu_Block_ViewMode)
	{
		ViewMenu->ViewMenu_Block_ViewMode->UpdateEnabledViewModes(Controller->ValidViewModes);
	}
}

void UEditModelUserWidget::EMOnSelectionObjectChanged()
{
	if (Controller->EMPlayerState->SelectedObjects.Num() == 0)
	{
		SelectionTrayWidget->CloseToolTray();
	}
	else
	{
		SelectionTrayWidget->OpenToolTrayForSelection();
	}
}

void UEditModelUserWidget::EditExistingAssembly(EToolMode ToolMode, const FGuid& AssemblyGUID)
{
	ToggleBIMDesigner(true);
	BIMDesigner->EditPresetInBIMDesigner(AssemblyGUID);
}

void UEditModelUserWidget::ToggleBIMDesigner(bool Open)
{
	if (Open)
	{
		BIMDesigner->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BIMBlockDialogBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		// TODO: open animation
	}
	else
	{
		// TODO: Replace with closing animation
		BIMDesigner->SetVisibility(ESlateVisibility::Collapsed);
		BIMBlockDialogBox->SetVisibility(ESlateVisibility::Collapsed);
		ToggleBIMPresetSwapTray(false);
		ScopeWarningWidget->DismissScopeWarning();
	}
	ToolTrayWidget->ToolTrayBIMDesignerMode(Open);
}

void UEditModelUserWidget::SwitchRightMenu(ERightMenuState NewMenuState)
{
	CurrentRightMenuState = NewMenuState;
	bool newViewMenuVisibility = CurrentRightMenuState == ERightMenuState::ViewMenu;
	bool newCutPlaneMenuVisibility = CurrentRightMenuState == ERightMenuState::CutPlaneMenu;
	ViewMenu->SetViewMenuVisibility(newViewMenuVisibility);
	CutPlaneMenu->SetCutPlaneMenuVisibility(newCutPlaneMenuVisibility);
}

void UEditModelUserWidget::UpdateCutPlanesList()
{
	if (CurrentRightMenuState == ERightMenuState::CutPlaneMenu)
	{
		CutPlaneMenu->UpdateCutPlaneMenuBlocks();
	}
}

bool UEditModelUserWidget::RemoveCutPlaneFromList(int32 ObjID /*= MOD_ID_NONE*/)
{
	if (CurrentRightMenuState == ERightMenuState::CutPlaneMenu)
	{
		return CutPlaneMenu->RemoveCutPlaneFromMenuBlock(ObjID);
	}
	return false;
}

bool UEditModelUserWidget::UpdateCutPlaneInList(int32 ObjID /*= MOD_ID_NONE*/)
{
	if (CurrentRightMenuState == ERightMenuState::CutPlaneMenu)
	{
		return CutPlaneMenu->UpdateCutPlaneParamInMenuBlock(ObjID);
	}
	return false;
}

void UEditModelUserWidget::RefreshAssemblyList()
{
	ToolTrayWidget->ToolTrayBlockAssembliesList->CreateAssembliesListForCurrentToolMode();
}

void UEditModelUserWidget::ShowAlertPausedAccountDialog()
{
	AlertPausedAccountDialogWidget->SetVisibility(ESlateVisibility::Visible);
}

void UEditModelUserWidget::ShowAlertFreeAccountDialog()
{
	AlertFreeAccountDialogWidget->SetVisibility(ESlateVisibility::Visible);
}

void UEditModelUserWidget::UpdateViewModeIndicator(EEditViewModes NewViewMode)
{
	if (ToolbarWidget != nullptr && ToolbarWidget->ToolBarTopBP != nullptr && ToolbarWidget->ToolBarTopBP->ViewModeIndicator != nullptr)
	{
		ToolbarWidget->ToolBarTopBP->ViewModeIndicator->SwitchToViewMode(NewViewMode);
	}

	if (ViewMenu && ViewMenu->ViewMenu_Block_ViewMode)
	{
		ViewMenu->ViewMenu_Block_ViewMode->SetActiveViewMode(NewViewMode);
	}
}

void UEditModelUserWidget::ShowBIMDebugger(bool NewVisible)
{
	BIMDebugger->SetVisibility(NewVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

bool UEditModelUserWidget::IsBIMDebuggerOn()
{
	return BIMDebugger->IsVisible();
}

void UEditModelUserWidget::ToggleBIMPresetSwapTray(bool NewVisibility)
{
	if (NewVisibility)
	{
		BIMPresetSwap->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ToolTrayWidget->SetVisibility(ESlateVisibility::Collapsed);
		SelectionTrayWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else if (Controller->GetToolMode() == EToolMode::VE_SELECT)
	{
		BIMPresetSwap->SetVisibility(ESlateVisibility::Collapsed);
		ToolTrayWidget->SetVisibility(ESlateVisibility::Collapsed);
		EMOnSelectionObjectChanged();
	}
	else if (Controller->GetToolMode() != EToolMode::VE_NONE)
	{
		BIMPresetSwap->SetVisibility(ESlateVisibility::Collapsed);
		SelectionTrayWidget->SetVisibility(ESlateVisibility::Collapsed);
		UpdateToolTray();
	}
}

void UEditModelUserWidget::ToggleTutorialMenu(bool NewVisibility)
{
	TutorialsMenuWidgetBP->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		ToolbarWidget->ToolBarTopBP->ButtonTopToolbarHelp->SwitchToActiveStyle();
	}
	else
	{
		ToolbarWidget->ToolBarTopBP->ButtonTopToolbarHelp->SwitchToNormalStyle();
	}
	// Test tutorial data
	TutorialsMenuWidgetBP->BuildTutorialMenuFromLink();
}
