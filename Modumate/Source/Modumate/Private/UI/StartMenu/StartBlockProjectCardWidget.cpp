// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartBlockProjectCardWidget.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "Components/Image.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"

UStartBlockProjectCardWidget::UStartBlockProjectCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStartBlockProjectCardWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	MainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode>();
	if (!ModumateButtonProjectCard)
	{
		return false;
	}
	ModumateButtonProjectCard->OnReleased.AddDynamic(this, &UStartBlockProjectCardWidget::OnButtonReleasedProjectCard);

	return true;
}

void UStartBlockProjectCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UStartBlockProjectCardWidget::OnButtonReleasedProjectCard()
{
	if (MainMenuGameMode && ProjectPath.Len() > 0)
	{
		MainMenuGameMode->OpenProject(ProjectPath, false);
	}
}

bool UStartBlockProjectCardWidget::BuildProjectCard(int32 ProjectID)
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
		TextBlockTitle->ChangeText(projectName);
		return true;
	}
	return false;
}
