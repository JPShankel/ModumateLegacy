// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"

#include "TutorialMenuCardWidget.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UTutorialMenuCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTutorialMenuCardWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class UAsyncTaskDownloadImage* ImageDownloadTask;

	FString ProjectFilePath;
	FString VideoLink;
	FString TutorialTitle;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TitleText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* DescriptionText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* VideoLengthText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonTutorialProject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonPlayVideo;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* ImageThumbnail;

	UFUNCTION()
	void OnReleaseButtonTutorialProject();

	UFUNCTION()
	void OnReleaseButtonPlayVideo();

	UFUNCTION()
	void OnImageDownloadedSucceed(class UTexture2DDynamic* Texture);

	UFUNCTION()
	void OnImageDownloadedFailed(class UTexture2DDynamic* Texture);

	void BuildTutorialCard(const FTutorialMenuInfo& InTutorialCard);
};
