// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialMenuCardWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Blueprint/AsyncTaskDownloadImage.h"
#include "Components/Image.h"

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

void UTutorialMenuCardWidget::OnImageDownloadedSucceed(class UTexture2DDynamic* Texture)
{
	ImageThumbnail->SetBrushFromTextureDynamic(Texture);
}

void UTutorialMenuCardWidget::OnImageDownloadedFailed(class UTexture2DDynamic* Texture)
{
	UE_LOG(LogTemp, Error, TEXT("Failed to download tutorial thumbnail for %s"), *TutorialTitle);
}

void UTutorialMenuCardWidget::BuildTutorialCard(const FTutorialMenuInfo& InTutorialCard)
{
	TutorialTitle = InTutorialCard.Title;
	TitleText->ChangeText(FText::FromString(TutorialTitle));
	DescriptionText->ChangeText(FText::FromString(InTutorialCard.Description));
	VideoLengthText->ChangeText(FText::FromString(InTutorialCard.VideoLength));
	ProjectFilePath = InTutorialCard.FileProject;
	VideoLink = InTutorialCard.VideoLink;

	if (!InTutorialCard.ThumbnailLink.IsEmpty())
	{
		ImageDownloadTask = UAsyncTaskDownloadImage::DownloadImage(InTutorialCard.ThumbnailLink);
		ImageDownloadTask->OnSuccess.AddDynamic(this, &UTutorialMenuCardWidget::OnImageDownloadedSucceed);
		ImageDownloadTask->OnFail.AddDynamic(this, &UTutorialMenuCardWidget::OnImageDownloadedFailed);
	}
}
