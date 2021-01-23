// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ToolbarTopWidget.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"


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

	if (!(ButtonModumateHome && ButtonTopToolbarHelp))
	{
		return false;
	}

	ButtonModumateHome->ModumateButton->OnReleased.AddDynamic(this, &UToolbarTopWidget::OnButtonReleaseModumateHome);
	ButtonTopToolbarHelp->ModumateButton->OnReleased.AddDynamic(this, &UToolbarTopWidget::OnButtonReleaseTopToolbarHelp);

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

void UToolbarTopWidget::OnButtonReleaseTopToolbarHelp()
{
	if (Controller && Controller->EditModelUserWidget)
	{
		bool isVisible = Controller->EditModelUserWidget->TutorialsMenuWidgetBP->IsVisible();
		Controller->EditModelUserWidget->ToggleTutorialMenu(!isVisible);
	}
}
