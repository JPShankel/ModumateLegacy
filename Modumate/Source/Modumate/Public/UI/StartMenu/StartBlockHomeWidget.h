// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartBlockHomeWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UStartBlockHomeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStartBlockHomeWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY()
	class AMainMenuGameMode *MainMenuGameMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWrapBox *WrapBoxProjects;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialMenuWidget* TutorialsMenuWidgetBP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UStartBlockProjectCardWidget> LoadProjectCardWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UStartBlockNewProjectCardWidget> NewProjectCardWidgetClass;

	void OpenRecentProjectMenu();
	void StartAbsoluteBeginners();

protected:
	virtual void NativeConstruct() override;

private:
	bool bHaveStartedBeginner = false;
};
