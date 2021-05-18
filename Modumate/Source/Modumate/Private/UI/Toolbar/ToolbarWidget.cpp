// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ToolbarWidget.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"


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

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	if (!(Controller && Button_Metaplanes && Button_Separators && Button_SurfaceGraphs && Button_Attachments && ButtonTopToolbarHelp && Button_SiteTools))
	{
		return false;
	}

	Button_Metaplanes->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseMetaPlane);
	Button_Separators->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseSeparators);
	Button_SurfaceGraphs->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseSurfaceGraphs);
	Button_Attachments->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseAttachments);
	Button_SiteTools->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseSiteTools);
	Button_3DViews->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonRelease3DViews);
	Button_CutPlanes->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseCutPlanes);
	ButtonTopToolbarHelp->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseTopToolbarHelp);
	Button_Browser->ModumateButton->OnReleased.AddDynamic(this, &UToolbarWidget::OnButtonReleaseBrowser);

	return true;
}

void UToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolbarWidget::OnButtonReleaseMetaPlane()
{
	Controller->SetToolMode(EToolMode::VE_LINE);
	if (EditModelUserWidget->ToolTrayWidget->CurrentToolCategory != EToolCategories::MetaGraph)
	{
		EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::ToolMenu, EToolCategories::MetaGraph);
	}
}

void UToolbarWidget::OnButtonReleaseSeparators()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::ToolMenu, EToolCategories::Separators);
	}
}

void UToolbarWidget::OnButtonReleaseSurfaceGraphs()
{
	Controller->SetToolMode(EToolMode::VE_SURFACEGRAPH);
	if (EditModelUserWidget->ToolTrayWidget->CurrentToolCategory != EToolCategories::SurfaceGraphs)
	{
		EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::ToolMenu, EToolCategories::SurfaceGraphs);
	}
}

void UToolbarWidget::OnButtonReleaseAttachments()
{
	if (EditModelUserWidget && (EditModelUserWidget->ToolTrayWidget))
	{
		EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::ToolMenu, EToolCategories::Attachments);
	}
}

void UToolbarWidget::OnButtonReleaseSiteTools()
{
	Controller->SetToolMode(EToolMode::VE_TERRAIN);
	if (EditModelUserWidget->ToolTrayWidget->CurrentToolCategory != EToolCategories::SiteTools)
	{
		EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::ToolMenu, EToolCategories::SiteTools);
	}
}

void UToolbarWidget::OnButtonRelease3DViews()
{
	Controller->SetToolMode(EToolMode::VE_SELECT);
	if (EditModelUserWidget)
	{
		if (EditModelUserWidget->CurrentLeftMenuState == ELeftMenuState::ViewMenu)
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::SelectMenu);
		}
		else
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::ViewMenu);
		}
	}
}

void UToolbarWidget::OnButtonReleaseCutPlanes()
{
	Controller->SetToolMode(EToolMode::VE_SELECT);
	if (EditModelUserWidget)
	{
		if (EditModelUserWidget->CurrentLeftMenuState == ELeftMenuState::CutPlaneMenu)
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::SelectMenu);
		}
		else
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::CutPlaneMenu);
		}
	}
}

void UToolbarWidget::OnButtonReleaseTopToolbarHelp()
{
	Controller->SetToolMode(EToolMode::VE_SELECT);
	if (EditModelUserWidget)
	{
		if (EditModelUserWidget->CurrentLeftMenuState == ELeftMenuState::TutorialMenu)
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::SelectMenu);
		}
		else
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::TutorialMenu);
		}
	}
}

void UToolbarWidget::OnButtonReleaseBrowser()
{
	Controller->SetToolMode(EToolMode::VE_SELECT);
	if (EditModelUserWidget)
	{
		if (EditModelUserWidget->CurrentLeftMenuState == ELeftMenuState::BrowserMenu)
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::SelectMenu);
		}
		else
		{
			EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::BrowserMenu);
		}
	}
}
