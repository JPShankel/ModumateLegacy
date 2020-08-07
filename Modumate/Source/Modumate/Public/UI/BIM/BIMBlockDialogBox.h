// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMBlockDialogBox.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockDialogBox : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockDialogBox(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_VariableText_GreyOutline;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_ActionText_Blue;

	UFUNCTION()
	void OnReleaseButton_VariableText_GreyOutline();

	UFUNCTION()
	void OnReleaseButton_ActionText_Blue();

protected:

};
