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
	switch (controller->GetToolMode())
	{
	case EToolMode::VE_METAPLANE:
		ToolTrayWidget->ChangeBlockToMetaPlaneTools();
		break;
	case EToolMode::VE_WALL:
	case EToolMode::VE_FLOOR:
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_RAIL:
	case EToolMode::VE_STAIR:
	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
	case EToolMode::VE_STRUCTURELINE:
		ToolTrayWidget->ChangeBlockToSeparatorTools();
		break;
	case EToolMode::VE_FINISH:
	case EToolMode::VE_TRIM:
	case EToolMode::VE_CABINET:
	case EToolMode::VE_COUNTERTOP:
	case EToolMode::VE_PLACEOBJECT:
		ToolTrayWidget->ChangeBlockToAttachmentTools();
		break;
	}
}
