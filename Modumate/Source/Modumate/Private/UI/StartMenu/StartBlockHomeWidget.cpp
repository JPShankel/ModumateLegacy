// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartBlockHomeWidget.h"
#include "UnrealClasses/MainMenuGameMode_CPP.h"
#include "Components/WrapBox.h"
#include "UI/StartMenu/StartBlockProjectCardWidget.h"
#include "UI/StartMenu/StartBlockNewProjectCardWidget.h"

UStartBlockHomeWidget::UStartBlockHomeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStartBlockHomeWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	MainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode_CPP>();

	return true;
}

void UStartBlockHomeWidget::OpenRecentProjectMenu()
{
	if (!(WrapBoxProjects && MainMenuGameMode))
	{
		return;
	}
	WrapBoxProjects->ClearChildren();

	UStartBlockNewProjectCardWidget *newProjectCard = CreateWidget<UStartBlockNewProjectCardWidget>(this, NewProjectCardWidgetClass);
	if (newProjectCard)
	{
		WrapBoxProjects->AddChildToWrapBox(newProjectCard);
	}

	for (int32 i = 0; i < MainMenuGameMode->NumRecentProjects; ++i)
	{
		UStartBlockProjectCardWidget *newLoadProjectCard = CreateWidget<UStartBlockProjectCardWidget>(this, LoadProjectCardWidgetClass);
		if (newLoadProjectCard && newLoadProjectCard->BuildProjectCard(i))
		{
			WrapBoxProjects->AddChildToWrapBox(newLoadProjectCard);
		}
	}
}

void UStartBlockHomeWidget::NativeConstruct()
{
	Super::NativeConstruct();
}