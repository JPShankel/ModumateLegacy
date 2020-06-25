// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ToolbarWidget.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ToolTray/ToolTrayWidget.h"



UToolbarWidget::UToolbarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolbarWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(Button_Metaplanes && Button_Separators && Button_SurfaceGraphs && Button_Attachments))
	{
		return false;
	}

	Button_Metaplanes->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonPressMetaPlane);
	Button_Separators->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonPressSeparators);
	Button_SurfaceGraphs->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonPressSurfaceGraphs);
	Button_Attachments->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonPressAttachments);

	return true;
}

void UToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolbarWidget::OnButtonPressMetaPlane()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToMetaPlaneTools();
	}
}

void UToolbarWidget::OnButtonPressSeparators()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToSeparatorTools();
	}
}

void UToolbarWidget::OnButtonPressSurfaceGraphs()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToSurfaceGraphTools();
	}
}

void UToolbarWidget::OnButtonPressAttachments()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToAttachmentTools();
	}
}
