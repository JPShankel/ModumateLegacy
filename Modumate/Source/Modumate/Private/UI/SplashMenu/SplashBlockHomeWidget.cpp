// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashBlockHomeWidget.h"
#include "UnrealClasses/MainMenuGameMode_CPP.h"
#include "Components/WrapBox.h"
#include "UI/SplashMenu/SplashBlockProjectCardWidget.h"
#include "UI/SplashMenu/SplashBlockNewProjectCardWidget.h"

USplashBlockHomeWidget::USplashBlockHomeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USplashBlockHomeWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	MainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode_CPP>();

	return true;
}

void USplashBlockHomeWidget::OpenRecentProjectMenu()
{
	if (!(WrapBoxProjects && MainMenuGameMode))
	{
		return;
	}
	WrapBoxProjects->ClearChildren();

	USplashBlockNewProjectCardWidget *newProjectCard = CreateWidget<USplashBlockNewProjectCardWidget>(this, NewProjectCardWidgetClass);
	if (newProjectCard)
	{
		WrapBoxProjects->AddChildToWrapBox(newProjectCard);
	}

	for (int32 i = 0; i < MainMenuGameMode->NumRecentProjects; ++i)
	{
		USplashBlockProjectCardWidget *newLoadProjectCard = CreateWidget<USplashBlockProjectCardWidget>(this, LoadProjectCardWidgetClass);
		if (newLoadProjectCard && newLoadProjectCard->BuildProjectCard(i))
		{
			WrapBoxProjects->AddChildToWrapBox(newLoadProjectCard);
		}
	}
}

void USplashBlockHomeWidget::NativeConstruct()
{
	Super::NativeConstruct();
}