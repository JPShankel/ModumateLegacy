// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/EditModelUserWidget.h"

#include "Online/ModumateCloudConnection.h"
#include "UI/BIM/BIMBlockDialogBox.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/ModumateSettingsMenu.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UI/RightMenu/CutPlaneMenuBlockExport.h"
#include "UI/RightMenu/ViewMenuBlockViewMode.h"
#include "UI/RightMenu/ViewMenuWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/Toolbar/ToolbarTopWidget.h"
#include "UI/Toolbar/ToolbarWidget.h"
#include "UI/Toolbar/ViewModeIndicatorWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/Debugger/BIMDebugger.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "UI/LeftMenu/BrowserMenuWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/LeftMenu/SwapMenuWidget.h"
#include "UI/LeftMenu/DeleteMenuWidget.h"

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
	ToolTrayWidget->NCPNavigatorAssembliesList->ResetSelectedAndSearchTag();
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

	if (NewState != CurrentLeftMenuState)
	{
		PreviousLeftMenuState = CurrentLeftMenuState;
		CurrentLeftMenuState = NewState;
	}

	bool newToolTrayVisibility = CurrentLeftMenuState == ELeftMenuState::ToolMenu;
	bool newViewMenuVisibility = CurrentLeftMenuState == ELeftMenuState::ViewMenu;
	bool newCutPlaneMenuVisibility = CurrentLeftMenuState == ELeftMenuState::CutPlaneMenu;
	bool newTutorialMenuVisibility = CurrentLeftMenuState == ELeftMenuState::TutorialMenu;
	bool newBrowserMenuVisibility = CurrentLeftMenuState == ELeftMenuState::BrowserMenu;
	bool newSwapMenuVisibility = CurrentLeftMenuState == ELeftMenuState::SwapMenu;
	bool newDeleteMenuVisibility = CurrentLeftMenuState == ELeftMenuState::DeleteMenu;
	newToolTrayVisibility ? ToolTrayWidget->OpenToolTray() : ToolTrayWidget->CloseToolTray();
	ToggleViewMenu(newViewMenuVisibility);
	ToggleCutPlaneMenu(newCutPlaneMenuVisibility);
	ToggleTutorialMenu(newTutorialMenuVisibility);
	ToggleBrowserMenu(newBrowserMenuVisibility);
	ToggleSwapMenu(newSwapMenuVisibility);
	ToggleDeleteMenu(newDeleteMenuVisibility);

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
		// ToolMenu requires specific category, check if it's unknown
		EToolCategories switchToToolCategory = AsToolCategory;
		if (switchToToolCategory == EToolCategories::Unknown)
		{
			switchToToolCategory = UModumateTypeStatics::GetToolCategory(Controller->GetToolMode());
		}

		switch (switchToToolCategory)
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
		case EToolCategories::SiteTools:
			ToolTrayWidget->ChangeBlockToSiteTools();
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

	// Close BIMDesigner if the new state isn't swap menu
	// Do not close BIM Designer on None, as it is handled by escape key
	if (!(NewState == ELeftMenuState::SwapMenu || NewState == ELeftMenuState::None))
	{
		ToggleBIMDesigner(false);
	}
}

void UEditModelUserWidget::EMOnSelectionObjectChanged()
{
	if (CurrentLeftMenuState == ELeftMenuState::SelectMenu ||
		CurrentLeftMenuState == ELeftMenuState::SwapMenu ||
		CurrentLeftMenuState == ELeftMenuState::ToolMenu ||
		CurrentLeftMenuState == ELeftMenuState::None)
	{
		if (!BIMDesigner->IsVisible())
		{
			SwitchLeftMenu(ELeftMenuState::SelectMenu);
		}
		else if (Controller->EMPlayerState->SelectedObjects.Num()==1)
		{
			// If the BIM designer is visible and we've made a single selection, edit it
			auto it = Controller->EMPlayerState->SelectedObjects.CreateConstIterator();
			if (it && (*it)->GetAssembly().PresetGUID != BIMDesigner->CraftingAssembly.PresetGUID)
			{
				BIMDesigner->EditPresetInBIMDesigner((*it)->GetAssembly().PresetGUID, false);
			}
		}
	}
}

void UEditModelUserWidget::EditExistingAssembly(const FGuid& AssemblyGUID)
{
	SwitchLeftMenu(ELeftMenuState::None);
	ToggleBIMDesigner(true);
	BIMDesigner->EditPresetInBIMDesigner(AssemblyGUID,true);
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
	}
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

void UEditModelUserWidget::RefreshAssemblyList(bool bScrollToSelected)
{
	ToolTrayWidget->NCPNavigatorAssembliesList->BuildAssemblyList(bScrollToSelected);
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
	// TODO: formalize escape handling and modality - any modal widget that intends to take focus should be able to handle escape here,
	// ideally without explicit enumeration here.
	if (SettingsMenuWidget->IsVisible())
	{
		ToggleSettingsWindow(false);
	}

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

void UEditModelUserWidget::ToggleSwapMenu(bool NewVisibility)
{
	SwapMenuWidget->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		SwapMenuWidget->BuildSwapMenu();
	}
}

void UEditModelUserWidget::ToggleDeleteMenu(bool NewVisibility)
{
	DeleteMenuWidget->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		DeleteMenuWidget->BuildDeleteMenu();
	}
}

void UEditModelUserWidget::ToggleSettingsWindow(bool NewVisibility)
{
	SettingsMenuWidget->SetVisibility(NewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (NewVisibility)
	{
		SettingsMenuWidget->UpdateFromCurrentSettings();
	}
}

#undef LOCTEXT_NAMESPACE
