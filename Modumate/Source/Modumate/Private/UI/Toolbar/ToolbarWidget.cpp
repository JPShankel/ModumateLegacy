// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ToolbarWidget.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"



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

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

	if (!(Controller && Button_Metaplanes && Button_Separators && Button_SurfaceGraphs && Button_Attachments))
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
	Controller->SetToolMode(EToolMode::VE_METAPLANE);
}

void UToolbarWidget::OnButtonPressSeparators()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToSeparatorTools(EToolMode::VE_NONE);
	}
}

void UToolbarWidget::OnButtonPressSurfaceGraphs()
{
	Controller->SetToolMode(EToolMode::VE_SURFACEGRAPH);
}

void UToolbarWidget::OnButtonPressAttachments()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToAttachmentTools(EToolMode::VE_NONE);
	}
}
