// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/EditModelUserWidget.h"
#include "UI/Toolbar/ToolbarWidget.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/BIM/BIMBlockDialogBox.h"
#include "UI/RightMenu/ViewMenuWidget.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"

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

	ToolbarWidget->EditModelUserWidget = this;
	ToolTrayWidget->EditModelUserWidget = this;

	return true;
}

void UEditModelUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UEditModelUserWidget::EMOnToolModeChanged()
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

void UEditModelUserWidget::EditExistingAssembly(EToolMode ToolMode, const FBIMKey& AssemblyKey)
{
	ToggleBIMDesigner(true);
	BIMDesigner->EditPresetInBIMDesigner(AssemblyKey);
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

void UEditModelUserWidget::SwitchRightMenu(ERightMenuState NewMenuState)
{
	CurrentRightMenuState = NewMenuState;
	bool newViewMenuVisibility = CurrentRightMenuState == ERightMenuState::ViewMenu;
	bool newCutPlaneMenuVisibility = CurrentRightMenuState == ERightMenuState::CutPlaneMenu;
	ViewMenu->SetViewMenuVisibility(newViewMenuVisibility);
	CutPlaneMenu->SetViewMenuVisibility(newCutPlaneMenuVisibility);
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

bool UEditModelUserWidget::UpdateCutPlaneVisibilityinList(bool IsVisible, int32 ObjID /*= MOD_ID_NONE*/)
{
	if (CurrentRightMenuState == ERightMenuState::CutPlaneMenu)
	{
		return CutPlaneMenu->UpdateCutPlaneVisibilityInMenuBlock(IsVisible, ObjID);
	}
	return false;
}

void UEditModelUserWidget::RefreshAssemblyList()
{
	ToolTrayWidget->ToolTrayBlockAssembliesList->CreateAssembliesListForCurrentToolMode();
}
