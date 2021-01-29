// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/TutorialManager.h"

#include "ModalDialogConfirmPlayTutorial.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModalDialogConfirmPlayTutorial : public UUserWidget
{
	GENERATED_BODY()

public:
	UModalDialogConfirmPlayTutorial(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	FString CurrentProjectFilePath;
	FString CurrentVideoLink;
	EModumateWalkthroughCategories CurrentWalkthroughCategory = EModumateWalkthroughCategories::None;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonOpenProject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonCancel;

	UFUNCTION()
	void OnReleaseButtonOpenProject();

	UFUNCTION()
	void OnReleaseButtonCancel();

	void BuildModalDialog(const FString& InProjectFilePath, const FString& InVideoLink);
	void BuildModalDialogWalkthrough(EModumateWalkthroughCategories InWalkthroughCategory);
};
