// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/IHttpRequest.h"

#include "TutorialMenuWidget.generated.h"

/**
 *
 */

static const TCHAR* DocObjectTutorialInfo = TEXT("TutorialInfoArrayCollection");

USTRUCT()
struct FTutorialMenuInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString Title;

	UPROPERTY()
	bool ShowTitleOnly = false;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	FString VideoLink;

	UPROPERTY()
	FString VideoLength;

	UPROPERTY()
	FString FileProject;

	UPROPERTY()
	FString ThumbnailLink;
};

USTRUCT()
struct FTutorialInfoArrayCollection
{
	GENERATED_BODY()

	UPROPERTY()
	FString HeaderMessage;

	UPROPERTY()
	TArray<FTutorialMenuInfo> ModumateTutorialInfoObjects;
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
	TArray<FTutorialMenuInfo> CurrentTutorialMenuCardInfo;

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
	FString JsonTutorialLink;

	void BuildTutorialMenuFromLink();
	void OnHttpReply(const FHttpRequestPtr& Request, const FHttpResponsePtr& Response, bool bWasSuccessful);
	void UpdateTutorialMenu(const FTutorialInfoArrayCollection& InTutorialInfo);
};
