// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SplashBlockHomeWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API USplashBlockHomeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USplashBlockHomeWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY()
	class AMainMenuGameMode_CPP *MainMenuGameMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWrapBox *WrapBoxProjects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class USplashBlockProjectCardWidget> LoadProjectCardWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class USplashBlockNewProjectCardWidget> NewProjectCardWidgetClass;

	void OpenRecentProjectMenu();

protected:
	virtual void NativeConstruct() override;
};
