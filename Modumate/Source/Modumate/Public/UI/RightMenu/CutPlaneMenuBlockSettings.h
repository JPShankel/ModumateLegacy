// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CutPlaneMenuBlockSettings.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UCutPlaneMenuBlockSettings : public UUserWidget
{
	GENERATED_BODY()

public:
	UCutPlaneMenuBlockSettings(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget *ButtonNewCutPlane;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget *ButtonShowHideAll;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget *ButtonExportSelected;

	UFUNCTION()
	void OnButtonNewCutPlaneReleased();

	UFUNCTION()
	void OnButtonShowHideAllReleased();

	UFUNCTION()
	void OnButtonExportSelectedReleased();
};