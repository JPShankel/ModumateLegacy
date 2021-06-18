// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "HelpBlockTutorial.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UHelpBlockTutorial : public UUserWidget
{
	GENERATED_BODY()

public:
	UHelpBlockTutorial(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget* Button_TutorialLibrary;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget* Button_IntroWalkthrough;

	UPROPERTY()
	class UHelpMenu* ParentHelpMenu;

	UFUNCTION()
	void OnButtonReleasedTutorialLibrary();

	UFUNCTION()
	void OnButtonReleasedIntroWalkthrough();
};
