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

	Button_Metaplanes->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseMetaPlane);
	Button_Separators->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseSeparators);
	Button_SurfaceGraphs->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseSurfaceGraphs);
	Button_Attachments->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseAttachments);
	Button_3DViews->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonRelease3DViews);
	Button_CutPlanes->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseCutPlanes);

	return true;
}

void UToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolbarWidget::OnButtonReleaseMetaPlane()
{
	Controller->SetToolMode(EToolMode::VE_METAPLANE);
	if (EditModelUserWidget->ToolTrayWidget->CurrentToolCategory != EToolCategories::MetaGraph)
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToMetaPlaneTools();
	}
}

void UToolbarWidget::OnButtonReleaseSeparators()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToSeparatorTools(EToolMode::VE_NONE);
	}
}

void UToolbarWidget::OnButtonReleaseSurfaceGraphs()
{
	Controller->SetToolMode(EToolMode::VE_SURFACEGRAPH);
	if (EditModelUserWidget->ToolTrayWidget->CurrentToolCategory != EToolCategories::SurfaceGraphs)
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToSurfaceGraphTools();
	}
}

void UToolbarWidget::OnButtonReleaseAttachments()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->ToolTrayWidget->ChangeBlockToAttachmentTools(EToolMode::VE_NONE);
	}
}

void UToolbarWidget::OnButtonRelease3DViews()
{
	if (EditModelUserWidget)
	{
		if (EditModelUserWidget->CurrentRightMenuState == ERightMenuState::ViewMenu)
		{
			EditModelUserWidget->SwitchRightMenu(ERightMenuState::None);
		}
		else
		{
			EditModelUserWidget->SwitchRightMenu(ERightMenuState::ViewMenu);
		}
	}
}

void UToolbarWidget::OnButtonReleaseCutPlanes()
{
	if (EditModelUserWidget)
	{
		if (EditModelUserWidget->CurrentRightMenuState == ERightMenuState::CutPlaneMenu)
		{
			EditModelUserWidget->SwitchRightMenu(ERightMenuState::None);
		}
		else
		{
			EditModelUserWidget->SwitchRightMenu(ERightMenuState::CutPlaneMenu);
		}
	}
}
