// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuController.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API AMainMenuController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UPROPERTY()
	class UStartRootMenuWidget *StartRootMenuWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UStartRootMenuWidget> StartRootMenuWidgetClass;
};
