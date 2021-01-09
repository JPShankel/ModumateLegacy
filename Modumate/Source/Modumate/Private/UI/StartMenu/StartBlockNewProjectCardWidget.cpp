// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartBlockNewProjectCardWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClasses/MainMenuController_CPP.h"
#include "UI/StartMenu/StartRootMenuWidget.h"

UStartBlockNewProjectCardWidget::UStartBlockNewProjectCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStartBlockNewProjectCardWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ModumateButtonNewProject)
	{
		return false;
	}
	ModumateButtonNewProject->OnReleased.AddDynamic(this, &UStartBlockNewProjectCardWidget::OnButtonReleasedNewProject);

	return true;
}

void UStartBlockNewProjectCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UStartBlockNewProjectCardWidget::OnButtonReleasedNewProject()
{
	AMainMenuController_CPP* controller = GetOwningPlayer<AMainMenuController_CPP>();
	if (controller && controller->StartRootMenuWidget)
	{
		UGameplayStatics::OpenLevel(this, controller->StartRootMenuWidget->NewLevelName, true);
	}
}