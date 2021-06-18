// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ToolbarTopWidget.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"


UToolbarTopWidget::UToolbarTopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolbarTopWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	if (!(ButtonModumateHome && ButtonHelp))
	{
		return false;
	}

	ButtonModumateHome->ModumateButton->OnReleased.AddDynamic(this, &UToolbarTopWidget::OnButtonReleaseModumateHome);
	ButtonHelp->ModumateButton->OnReleased.AddDynamic(this, &UToolbarTopWidget::OnButtonReleaseButtonHelp);

	return true;
}

void UToolbarTopWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolbarTopWidget::OnButtonReleaseModumateHome()
{
	if (Controller)
	{
		Controller->EditModelUserWidget->EventToggleProjectMenu();
	}
}

void UToolbarTopWidget::OnButtonReleaseButtonHelp()
{
	if (Controller)
	{
		Controller->EditModelUserWidget->ToggleHelpMenu(!Controller->EditModelUserWidget->bIsHelpMenuVisible);
	}
}

void UToolbarTopWidget::SwitchToViewMode(EEditViewModes NewViewMode)
{
	NewViewMode == EEditViewModes::MetaGraph ? Button_ViewModeMetaGraph->SwitchToActiveStyle() : Button_ViewModeMetaGraph->SwitchToNormalStyle();
	NewViewMode == EEditViewModes::Separators ? Button_ViewModeSeparator->SwitchToActiveStyle() : Button_ViewModeSeparator->SwitchToNormalStyle();
	NewViewMode == EEditViewModes::SurfaceGraphs ? Button_ViewModeSurfaceGraph->SwitchToActiveStyle() : Button_ViewModeSurfaceGraph->SwitchToNormalStyle();
	NewViewMode == EEditViewModes::AllObjects ? Button_ViewModeAllObject->SwitchToActiveStyle() : Button_ViewModeAllObject->SwitchToNormalStyle();
	NewViewMode == EEditViewModes::Physical ? Button_ViewModePhysical->SwitchToActiveStyle() : Button_ViewModePhysical->SwitchToNormalStyle();
}
