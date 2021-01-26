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
	Intro,
	WalkthroughSteps,
	Outro,
	None
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

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialWalkthroughBlockIntro* IntroWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialWalkthroughBlockItem* WalkthroughItemWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialWalkthroughBlockOutro* OutroWidget;

	UFUNCTION(BlueprintCallable)
	void ShowWalkthroughIntro();

	UFUNCTION(BlueprintCallable)
	void ShowWalkthroughStep(const FText& Title, const FText& Description, const FString& VideoURL, float ProgressPCT);
	
	UFUNCTION(BlueprintCallable)
	void ShowCountdown(float CountdownSeconds);

	UFUNCTION(BlueprintCallable)
	void ShowWalkthroughOutro();

	void UpdateBlockVisibility(ETutorialWalkthroughBlockStage Stage);

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

};
