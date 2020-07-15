// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashBlockProjectCardWidget.h"
#include "UnrealClasses/MainMenuGameMode_CPP.h"
#include "Components/Image.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateButton.h"

USplashBlockProjectCardWidget::USplashBlockProjectCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USplashBlockProjectCardWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	MainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode_CPP>();
	if (!ModumateButtonProjectCard)
	{
		return false;
	}
	ModumateButtonProjectCard->OnReleased.AddDynamic(this, &USplashBlockProjectCardWidget::OnButtonReleasedProjectCard);

	return true;
}

void USplashBlockProjectCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USplashBlockProjectCardWidget::OnButtonReleasedProjectCard()
{
	if (MainMenuGameMode && ProjectPath.Len() > 0)
	{
		MainMenuGameMode->OpenProject(ProjectPath);
	}
}

bool USplashBlockProjectCardWidget::BuildProjectCard(int32 ProjectID)
{
	if (!(MainMenuGameMode && ImageScene && TextBlockDate && TextBlockTitle))
	{
		return false;
	}
	FText projectName;
	FDateTime projectTime;
	FSlateBrush defaultThumbnail;
	FSlateBrush hoveredThumbnail;

	if (MainMenuGameMode->GetRecentProjectData(ProjectID, ProjectPath, projectName, projectTime, defaultThumbnail, hoveredThumbnail))
	{
		ImageScene->SetBrush(defaultThumbnail);
		TextBlockDate->SetText(FText::AsDateTime(projectTime, EDateTimeStyle::Default, EDateTimeStyle::Default, FText::GetInvariantTimeZone()));
		TextBlockTitle->SetText(projectName);
		return true;
	}
	return false;
}
