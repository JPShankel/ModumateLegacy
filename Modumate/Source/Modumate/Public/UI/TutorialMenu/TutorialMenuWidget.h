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
	class UModumateTutorialManager* TutorialManager;

	UPROPERTY()
	TArray<FTutorialMenuInfo> CurrentTutorialMenuCardInfo;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USizeBox* TitleHeader;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TutorialTitleHeader;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TutorialHeaderMessageText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* VerticalBoxTutorialCards;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModalDialogConfirmPlayTutorial* ModalDialogConfirmPlayTutorialBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonBeginnerWalkthrough;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonIntermediateWalkthrough;

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
	FText DefaultTutorialHeaderMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EllipsizeTitleWordAt = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString JsonTutorialLink;

	UFUNCTION()
	void OnReleaseButtonBeginnerWalkthrough();

	UFUNCTION()
	void OnReleaseButtonIntermediateWalkthrough();

	void BuildTutorialMenu();
	void OnHttpReply(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);	
	void UpdateTutorialMenu(const FTutorialInfoArrayCollection& InTutorialInfo);
	bool GetTutorialFilePath(const FString& TutorialFileName, FString& OutFullTutorialFilePath );
};
