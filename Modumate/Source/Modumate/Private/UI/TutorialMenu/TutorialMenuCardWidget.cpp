// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialMenuCardWidget.h"

#include "Blueprint/AsyncTaskDownloadImage.h"
#include "Components/Image.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/ModalDialog/ModalDialogConfirmPlayTutorial.h"
#include "UI/TutorialManager.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/MainMenuGameMode.h"

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
	// If this is opened in the main app, build a confirmation dialog so the user has an opportunity to save their work
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && ensure(ParentTutorialMenu && ParentTutorialMenu->ModalDialogConfirmPlayTutorialBP))
	{
		ParentTutorialMenu->ModalDialogConfirmPlayTutorialBP->BuildModalDialog(ProjectFilePath, VideoLink);
	}
	else if (ensure(ParentTutorialMenu && ParentTutorialMenu->TutorialManager)) // otherwise, open the video tutorial immediately
	{
		ParentTutorialMenu->TutorialManager->OpenVideoTutorial(ProjectFilePath, VideoLink);
	}
}

void UTutorialMenuCardWidget::OnReleaseButtonPlayVideo()
{
	if (ensure(ParentTutorialMenu && ParentTutorialMenu->TutorialManager))
	{
		ParentTutorialMenu->TutorialManager->OpenVideoTutorial(FString(), VideoLink);
	}
}

void UTutorialMenuCardWidget::OnImageDownloadedSucceed(class UTexture2DDynamic* Texture)
{
	if (ensure(Texture))
	{
		DownloadedTexture = Texture;
		ImageThumbnail->SetBrushFromTextureDynamic(DownloadedTexture);
	}
}

void UTutorialMenuCardWidget::OnImageDownloadedFailed(class UTexture2DDynamic* Texture)
{
	UE_LOG(LogTemp, Error, TEXT("Failed to download tutorial thumbnail for %s"), *TutorialTitle);
}

void UTutorialMenuCardWidget::BuildTutorialCard(const FTutorialMenuInfo& InTutorialCard, class UTutorialMenuWidget* InParentTutorialMenu)
{
	ParentTutorialMenu = InParentTutorialMenu;
	TutorialTitle = InTutorialCard.Title;
	TitleText->ChangeText(FText::FromString(TutorialTitle));
	DescriptionText->ChangeText(FText::FromString(InTutorialCard.Description));
	VideoLengthText->ChangeText(FText::FromString(InTutorialCard.VideoLength));
	VideoLink = InTutorialCard.VideoLink;

	// Hide play project button if file is invalid
	if (ensure(ParentTutorialMenu->TutorialManager))
	{
		bool bValidProjectFile = ParentTutorialMenu->TutorialManager->GetTutorialFilePath(InTutorialCard.FileProject, ProjectFilePath);
		ButtonTutorialProject->SetVisibility(bValidProjectFile ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

		if (!InTutorialCard.ThumbnailLink.IsEmpty())
		{
			ImageDownloadTask = UAsyncTaskDownloadImage::DownloadImage(InTutorialCard.ThumbnailLink);
			ImageDownloadTask->OnSuccess.AddDynamic(this, &UTutorialMenuCardWidget::OnImageDownloadedSucceed);
			ImageDownloadTask->OnFail.AddDynamic(this, &UTutorialMenuCardWidget::OnImageDownloadedFailed);
		}
	}
}
