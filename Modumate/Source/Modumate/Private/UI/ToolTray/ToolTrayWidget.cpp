// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/Custom//ModumateTextBlock.h"
#include "UI/ToolTray/ToolTrayBlockTools.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/ToolTray/ToolTrayBlockModes.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"

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

	return true;
}

void UToolTrayWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

bool UToolTrayWidget::ChangeBlockToMetaPlaneTools()
{
	ToolTrayMainTitleBlock->SetText(FText::FromString("Meta-Plane Tools"));
	HideAllToolTrayBlocks();
	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToMetaPlaneToolsButtons();
	return true;
}

bool UToolTrayWidget::ChangeBlockToSeparatorTools()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller)
	{
		ToolTrayMainTitleBlock->SetText(
			UModumateTypeStatics::GetTextForObjectType(
				UModumateTypeStatics::ObjectTypeFromToolMode(controller->GetToolMode())));
	}
	HideAllToolTrayBlocks();

	ToolTrayBlockTools->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockTools->ChangeToSeparatorToolsButtons();

	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToSeparatorToolsButtons();

	ToolTrayBlockAssembliesList->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockAssembliesList->CreateAssembliesListForCurrentToolMode();

	return true;
}

bool UToolTrayWidget::ChangeBlockToSurfaceGraphTools()
{
	ToolTrayMainTitleBlock->SetText(FText::FromString("Surface Graph Tools"));
	HideAllToolTrayBlocks();
	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToMetaPlaneToolsButtons();
	return true;
}

bool UToolTrayWidget::ChangeBlockToAttachmentTools()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller)
	{
		ToolTrayMainTitleBlock->SetText(
			UModumateTypeStatics::GetTextForObjectType(
				UModumateTypeStatics::ObjectTypeFromToolMode(controller->GetToolMode())));
	}
	HideAllToolTrayBlocks();

	ToolTrayBlockTools->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockTools->ChangeToAttachmentToolsButtons();

	ToolTrayBlockModes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ToolTrayBlockModes->ChangeToSeparatorToolsButtons();

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
}
