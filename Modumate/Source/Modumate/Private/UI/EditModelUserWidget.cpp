// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/EditModelUserWidget.h"

#include "Components/Border.h"
#include "Online/ModumateCloudConnection.h"
#include "UI/BIM/BIMBlockDialogBox.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/ModalDialog/AlertAccountDialogWidget.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UI/RightMenu/CutPlaneMenuBlockExport.h"
#include "UI/RightMenu/ViewMenuBlockViewMode.h"
#include "UI/RightMenu/ViewMenuWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/Toolbar/ToolbarTopWidget.h"
#include "UI/Toolbar/ToolbarWidget.h"
#include "UI/Toolbar/ViewModeIndicatorWidget.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/Debugger/BIMDebugger.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "UI/BIM/BIMScopeWarning.h"
#include "UI/LeftMenu/BrowserMenuWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"

#define LOCTEXT_NAMESPACE "ModumateWidgets"

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

	Controller = GetOwningPlayer<AEditModelPlayerController>();
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
	// Most toolmodes use tooltray menu, but cutplane uses its own cutplane menu
	if (Controller->GetToolMode() == EToolMode::VE_CUTPLANE)
	{
		SwitchLeftMenu(ELeftMenuState::CutPlaneMenu);
	}
	else if (Controller->GetToolMode() == EToolMode::VE_SELECT)
	{
		SwitchLeftMenu(ELeftMenuState::SelectMenu);
	}
	else
	{
		// Determine which category is the current toolmode in
		// Select, Move, etc do not use tool tray
		EToolCategories newToolCategory = UModumateTypeStatics::GetToolCategory(Controller->GetToolMode());
		if (newToolCategory != EToolCategories::Unknown)
		{
			SwitchLeftMenu(ELeftMenuState::ToolMenu, newToolCategory);
		}
		else
		{
			SwitchLeftMenu(ELeftMenuState::None);
		}
	}
}

void UEditModelUserWidget::SwitchLeftMenu(ELeftMenuState NewState, EToolCategories AsToolCategory)
{
	if (!(Controller && ToolTrayWidget))
	{
		return;
	}

	CurrentLeftMenuState = NewState;

	bool newToolTrayVisibility = CurrentLeftMenuState == ELeftMenuState::ToolMenu;
	bool newViewMenuVisibility = CurrentLeftMenuState == ELeftMenuState::ViewMenu;
	bool newCutPlaneMenuVisibility = CurrentLeftMenuState == ELeftMenuState::CutPlaneMenu;
	bool newTutorialMenuVisibility = CurrentLeftMenuState == ELeftMenuState::TutorialMenu;
	bool newBrowserMenuVisibility = CurrentLeftMenuState == ELeftMenuState::BrowserMenu;
	newToolTrayVisibility ? ToolTrayWidget->OpenToolTray() : ToolTrayWidget->CloseToolTray();
	ToggleViewMenu(newViewMenuVisibility);
	ToggleCutPlaneMenu(newCutPlaneMenuVisibility);
	ToggleTutorialMenu(newTutorialMenuVisibility);
	ToggleBrowserMenu(newBrowserMenuVisibility);

	if (NewState == ELeftMenuState::SelectMenu)
	{
		UpdateSelectTrayVisibility();
	}
	else
	{
		SelectionTrayWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (newToolTrayVisibility)
	{
		switch (AsToolCategory)
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
		}
	}

	// Change tool buttons visual style
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

	if (newViewMenuVisibility && ViewMenu && ViewMenu->ViewMenu_Block_ViewMode)
	{
		ViewMenu->ViewMenu_Block_ViewMode->UpdateEnabledViewModes(Controller->ValidViewModes);
	}

	// Close BIMDesigner if the new state isn't select menu
	if (NewState != ELeftMenuState::SelectMenu &&
		BIMDesigner->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ToggleBIMDesigner(false);
	}
}

void UEditModelUserWidget::EMOnSelectionObjectChanged()
{
	if (CurrentLeftMenuState == ELeftMenuState::SelectMenu ||
		CurrentLeftMenuState == ELeftMenuState::None)
	{
		SwitchLeftMenu(ELeftMenuState::SelectMenu);
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

void UEditModelUserWidget::UpdateCutPlanesList()
{
	if (CurrentLeftMenuState == ELeftMenuState::CutPlaneMenu)
	{
		CutPlaneMenu->UpdateCutPlaneMenuBlocks();
	}
}

bool UEditModelUserWidget::RemoveCutPlaneFromList(int32 ObjID /*= MOD_ID_NONE*/)
{
	if (CurrentLeftMenuState == ELeftMenuState::CutPlaneMenu)
	{
		return CutPlaneMenu->RemoveCutPlaneFromMenuBlock(ObjID);
	}
	return false;
}

bool UEditModelUserWidget::UpdateCutPlaneInList(int32 ObjID /*= MOD_ID_NONE*/)
{
	if (CurrentLeftMenuState == ELeftMenuState::CutPlaneMenu)
	{
		return CutPlaneMenu->UpdateCutPlaneParamInMenuBlock(ObjID);
	}
	return false;
}

void UEditModelUserWidget::RefreshAssemblyList()
{
	ToolTrayWidget->ToolTrayBlockAssembliesList->CreateAssembliesListForCurrentToolMode();
}

void UEditModelUserWidget::ShowAlertFreeAccountDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& ConfirmCallback)
{
	AlertFreeAccountDialogWidget->ShowDialog(AlertText, ConfirmText, ConfirmCallback);
}

void UEditModelUserWidget::UpdateViewModeIndicator(EEditViewModes NewViewMode)
{
	if (ToolbarWidget != nullptr && ToolbarWidget->ToolBarTopBP != nullptr)
	{
		ToolbarWidget->ToolBarTopBP->SwitchToViewMode(NewViewMode);
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

bool UEditModelUserWidget::IsBIMDesingerActive() const
{
	return BIMDesigner->IsVisible();
}

bool UEditModelUserWidget::EMUserWidgetHandleEscapeKey()
{
	if (CurrentLeftMenuState != ELeftMenuState::None)
	{
		SwitchLeftMenu(ELeftMenuState::None);
		return true;
	}
	return false;
}

void UEditModelUserWidget::UpdateSelectTrayVisibility()
{
	if (Controller->EMPlayerState->SelectedObjects.Num() == 0)
	{
		SelectionTrayWidget->CloseToolTray();
	}
	else
	{
		SelectionTrayWidget->UpdateFromSelection();
	}
}

FText UEditModelUserWidget::GetPlanUpgradeRichText()
{
	auto gameInstance = Controller->GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (!ensure(cloudConnection.IsValid()))
	{
		return FText::GetEmpty();
	}

	FText upgradeTextFormat = LOCTEXT("PlanUpgradeFormat", "<a href=\"{0}\">Upgrade</> your plan for unlimited exports.");
	static const FString upgradeURLSuffix(TEXT("workspace/plans"));
	FString upgradeURLFull = cloudConnection->GetCloudRootURL() / upgradeURLSuffix;
	return FText::Format(upgradeTextFormat, FText::FromString(upgradeURLFull));
}

void UEditModelUserWidget::ToggleBIMPresetSwapTray(bool NewVisibility)
{
	// TODO: Closing swap tray should look back to previous left menu state

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
		SelectionTrayWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else if (Controller->GetToolMode() != EToolMode::VE_NONE)
	{
		BIMPresetSwap->SetVisibility(ESlateVisibility::Collapsed);
		ToolTrayWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SelectionTrayWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UEditModelUserWidget::ToggleTutorialMenu(bool NewVisibility)
{
	TutorialsMenuWidgetBP->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		ToolbarWidget->ButtonTopToolbarHelp->SwitchToActiveStyle();
		TutorialsMenuWidgetBP->BuildTutorialMenu();
	}
	else
	{
		ToolbarWidget->ButtonTopToolbarHelp->SwitchToNormalStyle();
	}
}

void UEditModelUserWidget::ToggleCutPlaneMenu(bool NewVisibility)
{
	CutPlaneMenu->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		ToolbarWidget->Button_CutPlanes->SwitchToActiveStyle();
		CutPlaneMenu->UpdateCutPlaneMenuBlocks();
	}
	else
	{
		ToolbarWidget->Button_CutPlanes->SwitchToNormalStyle();
		CutPlaneMenu->CutPlaneMenuBlockExport->SetExportMenuVisibility(false);
	}
}

void UEditModelUserWidget::ToggleViewMenu(bool NewVisibility)
{
	ViewMenu->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		ToolbarWidget->Button_3DViews->SwitchToActiveStyle();
		ViewMenu->BuildViewMenu();
	}
	else
	{
		ToolbarWidget->Button_3DViews->SwitchToNormalStyle();
	}
}

void UEditModelUserWidget::ToggleBrowserMenu(bool NewVisibility)
{
	BrowserMenuWidget->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		ToolbarWidget->Button_Browser->SwitchToActiveStyle();
		BrowserMenuWidget->BuildBrowserMenu();
	}
	else
	{
		ToolbarWidget->Button_Browser->SwitchToNormalStyle();
	}
}

#undef LOCTEXT_NAMESPACE
