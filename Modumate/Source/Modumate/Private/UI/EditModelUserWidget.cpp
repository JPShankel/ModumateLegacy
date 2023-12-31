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
#include "UI/Toolbar/ToolbarBottomWidget.h"
#include "UI/Toolbar/ViewModeIndicatorWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UI/Debugger/BIMDebugger.h"
#include "UI/LeftMenu/BrowserMenuWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/LeftMenu/SwapMenuWidget.h"
#include "UI/LeftMenu/DeleteMenuWidget.h"
#include "UI/TutorialMenu/HelpMenu.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/Chat/ModumateChatWidget.h"
#include "UI/UsersList/UsersListHorizontalWidget.h"
#include "UI/UsersList/UsersListVerticalWidget.h"
#include "UI/DrawingDesigner/DrawingDesignerWebBrowserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/ViewCubeWidget.h"

#define LOCTEXT_NAMESPACE "ModumateWidgets"

TAutoConsoleVariable<bool> CVarModumateShowDrawingDesigner(TEXT("modumate.ShowDrawingDesigner"), 1, TEXT("Show the Drawing Designer bottom toolbar"), ECVF_Default);

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
	if (!(ProjectSystemMenu && ToolTrayWidget && ToolTrayWidget))
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

	bIsShowDrawingDesigner = CVarModumateShowDrawingDesigner.GetValueOnAnyThread();

	TextChatWidget->SetVisibility(bIsShowDrawingDesigner ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	ToolbarWidget->ToolbarBottomBlock->SetVisibility(bIsShowDrawingDesigner ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	
	// Switch toolbar visibility, webUI uses its own toolbar and tooltray
	if(bIsShowDrawingDesigner)
	{
		ToolbarWidget->SetVisibility(ESlateVisibility::Collapsed);
		ToolTrayWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	// DrawingDesigner widget
	DrawingDesigner->SetVisibility(bIsShowDrawingDesigner ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	UCanvasPanelSlot* drawingDesignerCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(DrawingDesigner);
	if (drawingDesignerCanvasSlot)
	{
		drawingDesignerCanvasSlot->SetOffsets(bIsShowDrawingDesigner ? FMargin(0.f) : FMargin(0.f, 48.f, 0.f, 40.f));
	}
	
	// BIM Designer
	UCanvasPanelSlot* BIMDesignerCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(BIMDesigner);
	if (BIMDesignerCanvasSlot)
	{
		BIMDesignerCanvasSlot->SetZOrder(bIsShowDrawingDesigner ? 8 : 1);
		if (bIsShowDrawingDesigner)
		{
			BIMDesignerCanvasSlot->SetOffsets(FMargin(0.f, 48.f, 0.f, 0.f));
		}
	}
	UCanvasPanelSlot* BIMBlockDialogBoxCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(BIMBlockDialogBox);
	if (BIMBlockDialogBoxCanvasSlot)
	{
		BIMBlockDialogBoxCanvasSlot->SetZOrder(bIsShowDrawingDesigner ? 8 : 1);
		BIMBlockDialogBoxCanvasSlot->SetOffsets(bIsShowDrawingDesigner ? FMargin(0.f, 48.f, 0.f, 0.f) : FMargin(56.f, 48.f, 0.f, 0.f));
	}

	// Delete menu
	UCanvasPanelSlot* DeleteMenuWidgetCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(DeleteMenuWidget);
	if (DeleteMenuWidgetCanvasSlot)
	{
		DeleteMenuWidgetCanvasSlot->SetZOrder(bIsShowDrawingDesigner ? 9 : 6);
		DeleteMenuWidgetCanvasSlot->SetPosition(bIsShowDrawingDesigner ? FVector2D(0.f, 48.f) : FVector2D(56.f, 48.f));
	}

	// Swap menu
	UCanvasPanelSlot* SwapMenuWidgetCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(SwapMenuWidget);
	if (SwapMenuWidgetCanvasSlot)
	{
		SwapMenuWidgetCanvasSlot->SetZOrder(bIsShowDrawingDesigner ? 9 : 6);
		SwapMenuWidgetCanvasSlot->SetPosition(bIsShowDrawingDesigner ? FVector2D(0.f, 48.f) : FVector2D(56.f, 48.f));
	}

	// Browser menu for quantity export
	UCanvasPanelSlot* BrowserMenuWidgetCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(BrowserMenuWidget);
	if (BrowserMenuWidgetCanvasSlot)
	{
		BrowserMenuWidgetCanvasSlot->SetZOrder(bIsShowDrawingDesigner ? 9 : 6);
		BrowserMenuWidgetCanvasSlot->SetPosition(bIsShowDrawingDesigner ? FVector2D(0.f, 48.f) : FVector2D(56.f, 48.f));
	}

	// ViewCube
	UpdateViewCubeOffset(bIsShowDrawingDesigner ? -300.f : -50.f);
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
	// New UI shouldn't change umg menu visibility, but with few exceptions
	if (bIsShowDrawingDesigner)
	{
		if (!(NewState == ELeftMenuState::SwapMenu ||
			NewState == ELeftMenuState::DeleteMenu ||
			NewState == ELeftMenuState::None))
		{
			return;
		}
	}

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
	bool newBrowserMenuVisibility = CurrentLeftMenuState == ELeftMenuState::BrowserMenu;
	bool newSwapMenuVisibility = CurrentLeftMenuState == ELeftMenuState::SwapMenu;
	bool newDeleteMenuVisibility = CurrentLeftMenuState == ELeftMenuState::DeleteMenu;
	newToolTrayVisibility ? ToolTrayWidget->OpenToolTray() : ToolTrayWidget->CloseToolTray();
	ToggleViewMenu(newViewMenuVisibility);
	ToggleCutPlaneMenu(newCutPlaneMenuVisibility);
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
	UpdateMoveRotateToolButtonsUsability();

	if (CurrentLeftMenuState == ELeftMenuState::SwapMenu)
	{
		if (!SwapMenuWidget->IsCurrentObjsSelectionValidForSwap())
		{
			SwitchLeftMenu(ELeftMenuState::SelectMenu);
		}
		return;
	}

	if (CurrentLeftMenuState == ELeftMenuState::SelectMenu ||
		CurrentLeftMenuState == ELeftMenuState::ToolMenu ||
		CurrentLeftMenuState == ELeftMenuState::None)
	{
		if (!BIMDesigner->IsVisible())
		{
			SwitchLeftMenu(ELeftMenuState::SelectMenu);
		}
		else if (Controller && Controller->EMPlayerState && Controller->EMPlayerState->SelectedObjects.Num()==1)
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
	if (!BIMDesigner->EditPresetInBIMDesigner(AssemblyGUID, true))
	{
		ToggleBIMDesigner(false);
	}
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

		if (Controller)
		{
			Controller->bResetFocusToGameViewport = true;	
		}
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

void UEditModelUserWidget::ToggleDatasmithDebugger()
{
	bool isHidden = DatasmithDebugger->GetVisibility() == ESlateVisibility::Collapsed;
	DatasmithDebugger->SetVisibility(isHidden ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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

	if (CurrentLeftMenuState != ELeftMenuState::None && CurrentLeftMenuState != ELeftMenuState::SelectMenu)
	{
		SwitchLeftMenu(ELeftMenuState::None);
		return true;
	}
	return false;
}

void UEditModelUserWidget::UpdateSelectTrayVisibility()
{
	if (Controller && Controller->EMPlayerState && Controller->EMPlayerState->SelectedObjects.Num() == 0)
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

void UEditModelUserWidget::UpdateMoveRotateToolButtonsUsability()
{
	auto moveButton = ToolToButtonMap.FindRef(EToolMode::VE_MOVEOBJECT);
	auto rotateButton = ToolToButtonMap.FindRef(EToolMode::VE_ROTATE);
	if (moveButton && rotateButton && Controller && Controller->EMPlayerState)
	{
		bool bSelectionValid = Controller->EMPlayerState->NumItemsSelected() > 0;
		moveButton->SetIsEnabled(bSelectionValid);
		rotateButton->SetIsEnabled(bSelectionValid);
	}
}

void UEditModelUserWidget::UpdateUsersList()
{
	auto* gameState = GetWorld() ? GetWorld()->GetGameState<AEditModelGameState>() : nullptr;
	if (!gameState)
	{
		return;
	}

	TArray<AEditModelPlayerState*> playersOnToolBar;
	for (const auto& curPS : gameState->PlayerArray)
	{
		AEditModelPlayerState* emPS = Cast<AEditModelPlayerState>(curPS);
		if (emPS && !emPS->IsActorBeingDestroyed())
		{
			playersOnToolBar.Add(emPS);
		}
	}

	ToolbarWidget->ToolBarTopBP->UsersListHorizontal_BP->UpdateHorizontalUsersList(playersOnToolBar);
}

void UEditModelUserWidget::EditDetailDesignerFromSelection()
{
	SelectionTrayWidget->StartDetailDesignerFromSelection();
}

void UEditModelUserWidget::UpdateViewCubeOffset(float InOffset)
{
	UCanvasPanelSlot* viewCubeCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ViewCubeUserWidget);
	if (viewCubeCanvasSlot)
	{
		FVector2D offset = FVector2D(InOffset, 100.f);
		viewCubeCanvasSlot->SetPosition(offset);
	}
}

void UEditModelUserWidget::CheckDeletePresetFromWebUI(const FGuid& PresetGUIDToDelete)
{
	// If this preset does not affect any other preset, then just display the modal dialog for delete
	TArray<FGuid> allAncestorPresets;
	if (!Controller->GetDocument()->PresetIsInUse(PresetGUIDToDelete))
	{
		Controller->GetDocument()->DeletePreset(GetWorld(), PresetGUIDToDelete, FGuid());
		return;
	}

	// Otherwise, this preset needs to be replaced
	FBIMTagPath ncpForReplace;
	Controller->GetDocument()->GetPresetCollection().GetNCPForPreset(PresetGUIDToDelete, ncpForReplace);
	if (ensureAlways(ncpForReplace.Tags.Num() > 0))
	{
		DeleteMenuWidget->NCPNavigatorWidget->ResetSelectedAndSearchTag();

		DeleteMenuWidget->SetPresetToDelete(PresetGUIDToDelete, FGuid());
		SwitchLeftMenu(ELeftMenuState::DeleteMenu);
		DeleteMenuWidget->NCPNavigatorWidget->SetNCPTagPathAsSelected(ncpForReplace);
	}
}

void UEditModelUserWidget::CheckUpdateSwapMenu()
{
	if (CurrentLeftMenuState == ELeftMenuState::SwapMenu)
	{
		ToggleSwapMenu(true);
	}
}

void UEditModelUserWidget::ToggleCutPlaneMenu(bool bNewVisibility)
{
	CutPlaneMenu->SetVisibility(bNewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bNewVisibility)
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

void UEditModelUserWidget::ToggleViewMenu(bool bNewVisibility)
{
	ViewMenu->SetVisibility(bNewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bNewVisibility)
	{
		ToolbarWidget->Button_3DViews->SwitchToActiveStyle();
		ViewMenu->BuildViewMenu();
	}
	else
	{
		ToolbarWidget->Button_3DViews->SwitchToNormalStyle();
	}
}

void UEditModelUserWidget::ToggleBrowserMenu(bool bNewVisibility)
{
	BrowserMenuWidget->SetVisibility(bNewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bNewVisibility)
	{
		ToolbarWidget->Button_Browser->SwitchToActiveStyle();
		BrowserMenuWidget->BuildBrowserMenu();
	}
	else
	{
		ToolbarWidget->Button_Browser->SwitchToNormalStyle();
	}
}

void UEditModelUserWidget::ToggleSwapMenu(bool bNewVisibility)
{
	SwapMenuWidget->SetVisibility(bNewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bNewVisibility)
	{
		SwapMenuWidget->BuildSwapMenu();
	}
}

void UEditModelUserWidget::ToggleDeleteMenu(bool bNewVisibility)
{
	DeleteMenuWidget->SetVisibility(bNewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bNewVisibility)
	{
		DeleteMenuWidget->BuildDeleteMenu();
	}
}

void UEditModelUserWidget::ToggleSettingsWindow(bool bNewVisibility)
{
	SettingsMenuWidget->SetVisibility(bNewVisibility ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bNewVisibility)
	{
		SettingsMenuWidget->UpdateSettingsFromDoc();
	}
}

void UEditModelUserWidget::ToggleHelpMenu(bool NewVisibility)
{
	HelpMenuBP->ResetHelpWebBrowser();
	bIsHelpMenuVisible = NewVisibility;
	HelpMenuBP->SetVisibility(bIsHelpMenuVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bIsHelpMenuVisible)
	{
		ToggleTextChat(false);
		ToggleVerticalUserList(false);
		HelpMenuBP->ToMainHelpMenu();
	}
	else
	{
		// Send analytic event
		UModumateAnalyticsStatics::RecordEventCustomString(this, EModumateAnalyticsCategory::Tutorials, UHelpMenu::AnalyticsSearchEvent,HelpMenuBP->GetHelpMenuSearchbarText());
	}
} 

void UEditModelUserWidget::ToggleTextChat(bool bNewVisibility)
{
	if (!bNewVisibility)
	{
		TextChatWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		TextChatWidget->SetVisibility(ESlateVisibility::Visible);
		ToggleHelpMenu(false);
		ToggleVerticalUserList(false);
	}
	
}

void UEditModelUserWidget::ToggleVerticalUserList(bool bNewVisibility)
{
	if (!bNewVisibility)
	{
		UsersListVertical->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UsersListVertical->SetVisibility(ESlateVisibility::Visible);
		ToggleTextChat(false);
		ToggleHelpMenu(false);
	}
}

void UEditModelUserWidget::ToggleViewOnlyBadge(bool bNewVisibility)
{
	if (ToolbarWidget->ToolBarTopBP->ViewOnlyBadge)
	{
		auto widget = ToolbarWidget->ToolBarTopBP->ViewOnlyBadge;
		if (bNewVisibility)
		{
			widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			widget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

#undef LOCTEXT_NAMESPACE
