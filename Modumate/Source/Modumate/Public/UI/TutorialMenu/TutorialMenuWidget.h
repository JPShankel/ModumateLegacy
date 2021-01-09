// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "TutorialMenuWidget.generated.h"

/**
 *
 */

USTRUCT(BlueprintType)
struct FTutorialMenuCardInfo
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere)
	FString Title;

	UPROPERTY(EditAnywhere)
	bool ShowTitleOnly = false;

	UPROPERTY(EditAnywhere)
	FString Description;

	UPROPERTY(EditAnywhere)
	FString VideoLink;

	UPROPERTY(EditAnywhere)
	FString VideoLength;

	UPROPERTY(EditAnywhere)
	FString FileProject;

	UPROPERTY(EditAnywhere)
	FString ThumbnailLink;
};

UCLASS()
class MODUMATE_API UTutorialMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTutorialMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TArray<FTutorialMenuCardInfo> CurrentTutorialMenuCardInfo;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USizeBox* TitleHeader;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TutorialTextHeader;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* VerticalBoxTutorialCards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool AsStartMenuTutorial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMargin TitleSectionMargin = FMargin(0.f, 0.f, 0.f, 12.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMargin CardMargin = FMargin(0.f, 0.f, 0.f, 12.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UTutorialMenuCardWidget> TutorialCardClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateTextBlockUserWidget> TutorialTitleTextClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EllipsizeTitleWordAt = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FTutorialMenuCardInfo> TestTutorialInfo;

	void BuildTutorialMenu(const TArray<FTutorialMenuCardInfo>& InTutorialCards);
};
