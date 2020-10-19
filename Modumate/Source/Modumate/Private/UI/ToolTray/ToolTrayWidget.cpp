// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayWidget.h"

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UI/Custom//ModumateTextBlock.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Toolbar/ToolbarWidget.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ToolTray/ToolTrayBlockModes.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/ToolTray/ToolTrayBlockTools.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

UToolTrayWidget::UToolTrayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolTrayWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ToolTrayBlockAssembliesList)
	{
		return false;
	}

	ToolTrayBlockAssembliesList->ToolTray = this;

	return true;
}

void UToolTrayWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

bool UToolTrayWidget::ChangeBlockToMetaPlaneTools()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!controller)
	{
		return false;
	}

	OpenToolTray();
	ToolTrayMainTitleBlock->SetText(UModumateTypeStatics::GetToolCategoryText(EToolCategories::MetaGraph));
	HideAllToolTrayBlocks();
	CurrentToolCategory = EToolCategories::MetaGraph;

	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToMetaPlaneToolsButtons();

	ToolTrayBlockProperties->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockProperties->ChangeBlockProperties(Cast<UEditModelToolBase>(controller->CurrentTool.GetObject()));

	if (EditModelUserWidget)
	{
		EditModelUserWidget->ToolbarWidget->Button_Metaplanes->SwitchToActiveStyle();
	}

	return true;
}

bool UToolTrayWidget::ChangeBlockToSeparatorTools(EToolMode Toolmode)
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!controller)
	{
		return false;
	}

	OpenToolTray();
	HideAllToolTrayBlocks();
	CurrentToolCategory = EToolCategories::Separators;
	ToolTrayBlockTools->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockTools->ChangeToSeparatorToolsButtons();
	if (EditModelUserWidget)
	{
		EditModelUserWidget->ToolbarWidget->Button_Separators->SwitchToActiveStyle();
	}
	if (UModumateTypeStatics::GetToolCategory(Toolmode) != CurrentToolCategory)
	{
		ToolTrayMainTitleBlock->SetText(UModumateTypeStatics::GetToolCategoryText(EToolCategories::Separators));
		return false;
	}
	ToolTrayMainTitleBlock->SetText(UModumateTypeStatics::GetTextForObjectType(UModumateTypeStatics::ObjectTypeFromToolMode(controller->GetToolMode())));

	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToSeparatorToolsButtons(controller->GetToolMode());

	ToolTrayBlockProperties->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockProperties->ChangeBlockProperties(Cast<UEditModelToolBase>(controller->CurrentTool.GetObject()));

	ToolTrayBlockAssembliesList->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockAssembliesList->CreateAssembliesListForCurrentToolMode();

	return true;
}

bool UToolTrayWidget::ChangeBlockToSurfaceGraphTools()
{
	OpenToolTray();
	CurrentToolCategory = EToolCategories::SurfaceGraphs;
	ToolTrayMainTitleBlock->SetText(UModumateTypeStatics::GetToolCategoryText(EToolCategories::SurfaceGraphs));
	HideAllToolTrayBlocks();
	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToSurfaceGraphToolsButtons();
	if (EditModelUserWidget)
	{
		EditModelUserWidget->ToolbarWidget->Button_SurfaceGraphs->SwitchToActiveStyle();
	}

	return true;
}

bool UToolTrayWidget::ChangeBlockToAttachmentTools(EToolMode Toolmode)
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!controller)
	{
		return false;
	}

	OpenToolTray();
	HideAllToolTrayBlocks();
	CurrentToolCategory = EToolCategories::Attachments;
	ToolTrayBlockTools->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockTools->ChangeToAttachmentToolsButtons();
	if (EditModelUserWidget)
	{
		EditModelUserWidget->ToolbarWidget->Button_Attachments->SwitchToActiveStyle();
	}
	if (UModumateTypeStatics::GetToolCategory(Toolmode) != CurrentToolCategory)
	{
		ToolTrayMainTitleBlock->SetText(UModumateTypeStatics::GetToolCategoryText(EToolCategories::Attachments));
		return false;
	}
	ToolTrayMainTitleBlock->SetText(UModumateTypeStatics::GetTextForObjectType(UModumateTypeStatics::ObjectTypeFromToolMode(controller->GetToolMode())));

	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToSeparatorToolsButtons(controller->GetToolMode());

	ToolTrayBlockAssembliesList->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockAssembliesList->CreateAssembliesListForCurrentToolMode();

	return true;
}

void UToolTrayWidget::HideAllToolTrayBlocks()
{
	ToolTrayBlockTools->SetVisibility(ESlateVisibility::Collapsed);
	ToolTrayBlockModes->SetVisibility(ESlateVisibility::Collapsed);
	ToolTrayBlockProperties->SetVisibility(ESlateVisibility::Collapsed);
	ToolTrayBlockAssembliesList->SetVisibility(ESlateVisibility::Collapsed);
	if (EditModelUserWidget)
	{
		EditModelUserWidget->ToolbarWidget->Button_Metaplanes->SwitchToNormalStyle();
		EditModelUserWidget->ToolbarWidget->Button_Separators->SwitchToNormalStyle();
		EditModelUserWidget->ToolbarWidget->Button_SurfaceGraphs->SwitchToNormalStyle();
		EditModelUserWidget->ToolbarWidget->Button_Attachments->SwitchToNormalStyle();
	}
}

bool UToolTrayWidget::IsToolTrayVisible()
{
	return (GetVisibility() == ESlateVisibility::SelfHitTestInvisible ||
		GetVisibility() == ESlateVisibility::Visible ||
		GetVisibility() == ESlateVisibility::HitTestInvisible);
}

void UToolTrayWidget::OpenToolTray()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// TODO: Set menu animation here
}

void UToolTrayWidget::CloseToolTray()
{
	// TODO: Set menu animation here
	SetVisibility(ESlateVisibility::Collapsed);
	HideAllToolTrayBlocks();
}
