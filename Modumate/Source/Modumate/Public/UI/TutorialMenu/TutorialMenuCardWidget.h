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

	FString ProjectFilePath;
	FString VideoLink;

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

	UFUNCTION()
	void OnReleaseButtonTutorialProject();

	UFUNCTION()
	void OnReleaseButtonPlayVideo();

	void BuildTutorialCard(const FTutorialMenuCardInfo& InTutorialCard);
};
