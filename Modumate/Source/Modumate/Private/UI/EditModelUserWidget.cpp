// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/EditModelUserWidget.h"
#include "UI/Toolbar/ToolbarWidget.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelInputHandler.h"

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
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!(controller && ToolTrayWidget))
	{
		return;
	}
	switch (UModumateTypeStatics::GetToolCategory(controller->GetToolMode()))
	{
	case EToolCategories::MetaGraph:
		ToolTrayWidget->ChangeBlockToMetaPlaneTools();
		break;
	case EToolCategories::Separators:
		ToolTrayWidget->ChangeBlockToSeparatorTools(controller->GetToolMode());
		break;
	case EToolCategories::SurfaceGraphs:
		ToolTrayWidget->ChangeBlockToSurfaceGraphTools();
		break;
	case EToolCategories::Attachments:
		ToolTrayWidget->ChangeBlockToAttachmentTools(controller->GetToolMode());
		break;
	default:
		ToolTrayWidget->CloseToolTray();
	}
}
