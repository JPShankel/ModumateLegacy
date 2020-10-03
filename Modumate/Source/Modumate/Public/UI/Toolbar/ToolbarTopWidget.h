// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ToolbarTopWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolbarTopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolbarTopWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY()
	class UEditModelUserWidget *EditModelUserWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonModumateHome;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UViewModeIndicatorWidget* ViewModeIndicator;

	UFUNCTION()
	void OnButtonReleaseModumateHome();

};
