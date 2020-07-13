// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SplashRootMenuWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API USplashRootMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USplashRootMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonTutorial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonQuit;
};
