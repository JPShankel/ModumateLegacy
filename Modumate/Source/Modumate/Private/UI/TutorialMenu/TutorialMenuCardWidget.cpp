// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialMenuCardWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"

UTutorialMenuCardWidget::UTutorialMenuCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTutorialMenuCardWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonTutorialProject && ButtonPlayVideo))
	{
		return false;
	}

	ButtonTutorialProject->ModumateButton->OnReleased.AddDynamic(this, &UTutorialMenuCardWidget::OnReleaseButtonTutorialProject);
	ButtonPlayVideo->ModumateButton->OnReleased.AddDynamic(this, &UTutorialMenuCardWidget::OnReleaseButtonPlayVideo);

	return true;
}

void UTutorialMenuCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UTutorialMenuCardWidget::OnReleaseButtonTutorialProject()
{

}

void UTutorialMenuCardWidget::OnReleaseButtonPlayVideo()
{
	FPlatformProcess::LaunchURL(*VideoLink, nullptr, nullptr);
}

void UTutorialMenuCardWidget::BuildTutorialCard(const FTutorialMenuCardInfo& InTutorialCard)
{
	TitleText->ChangeText(FText::FromString(InTutorialCard.Title));
	DescriptionText->ChangeText(FText::FromString(InTutorialCard.Description));
	VideoLengthText->ChangeText(FText::FromString(InTutorialCard.VideoLength));
	ProjectFilePath = InTutorialCard.FileProject;
	VideoLink = InTutorialCard.VideoLink;
}
