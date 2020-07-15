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

	UPROPERTY()
	class UModumateGameInstance *ModumateGameInstance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString HelpURL;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonHelp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonQuit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USplashBlockHomeWidget *Splash_Home_BP;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void OnButtonReleasedQuit();

	UFUNCTION()
	void OnButtonReleasedHelp();
};
