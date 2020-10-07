// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnrealClasses/EditModelInputHandler.h"

#include "ModumateButtonUserWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateButtonUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateButtonUserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY()
	FButtonStyle NormalButtonStyle;

	UPROPERTY()
	FButtonStyle ActiveButtonStyle;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EInputCommand InputCommand;

	UFUNCTION()
	void OnButtonPress();

	void SwitchToNormalStyle();
	void SwitchToActiveStyle();
};
