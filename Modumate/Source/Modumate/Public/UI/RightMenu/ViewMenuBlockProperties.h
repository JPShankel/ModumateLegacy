// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ViewMenuBlockProperties.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UViewMenuBlockProperties : public UUserWidget
{
	GENERATED_BODY()

public:
	UViewMenuBlockProperties(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox *ControlGravityOn;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox *ControlGravityOff;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBox_FOV;

	UFUNCTION()
	void OnEditableTextBoxFOVCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnControlGravityOnChanged(bool IsChecked);

	UFUNCTION()
	void OnControlGravityOffChanged(bool IsChecked);

	void ToggleGravityCheckboxes(bool NewEnable);
};
