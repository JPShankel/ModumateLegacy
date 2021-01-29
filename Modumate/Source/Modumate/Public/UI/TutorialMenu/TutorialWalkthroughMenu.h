// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "TutorialWalkthroughMenu.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class ETutorialWalkthroughBlockStage : uint8
{
	None,
	Intro,
	WalkthroughSteps,
	Outro
};

UCLASS()
class MODUMATE_API UTutorialWalkthroughMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UTutorialWalkthroughMenu(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	FTimerHandle TutorialCountdownTimer;
	int32 TutorialCountdownStep = 0;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialWalkthroughBlockIntro* IntroWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialWalkthroughBlockItem* WalkthroughItemWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialWalkthroughBlockOutro* OutroWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UMediaPlayer* MediaPlayer;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UStreamMediaSource* StreamMediaSource;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString CountdownPrefix;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString CountdownSuffix;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText WalkthroughNextText;

	UFUNCTION(BlueprintCallable)
	void ShowWalkthroughIntro(const FText& Title, const FText& Description);

	UFUNCTION(BlueprintCallable)
	void ShowWalkthroughStep(const FText& Title, const FText& Description, const FString& VideoURL, float ProgressPCT);
	
	UFUNCTION(BlueprintCallable)
	void ShowCountdown(float CountdownSeconds);

	UFUNCTION(BlueprintCallable)
	void ShowWalkthroughOutro(const FText& Title, const FText& Description);

	UFUNCTION()
	void CheckTutorialCountdownTimer();

	UFUNCTION()
	void UpdateCountdownText();

	void UpdateBlockVisibility(ETutorialWalkthroughBlockStage Stage);

	UFUNCTION()
	void OnUrlMediaOpened(FString Url);

	UFUNCTION()
	void OnReleaseButtonIntroProceed();

	UFUNCTION()
	void OnReleaseButtonIntroGoBack();

	UFUNCTION()
	void OnReleaseWalkthroughButtonProceed();

	UFUNCTION()
	void OnReleaseButtonWalkthroughGoBack();

	UFUNCTION()
	void OnReleaseButtonOutroProceed();

	UFUNCTION()
	void OnReleaseButtonOutroGoBack();

protected:
	UPROPERTY()
	class UModumateTutorialManager* TutorialManager;
};
