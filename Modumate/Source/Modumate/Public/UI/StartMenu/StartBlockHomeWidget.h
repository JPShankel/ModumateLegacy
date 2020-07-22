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
	class AMainMenuGameMode_CPP *MainMenuGameMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWrapBox *WrapBoxProjects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UStartBlockProjectCardWidget> LoadProjectCardWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UStartBlockNewProjectCardWidget> NewProjectCardWidgetClass;

	void OpenRecentProjectMenu();

protected:
	virtual void NativeConstruct() override;
};
