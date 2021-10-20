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
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY()
	class AEditModelPlayerController *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* GravityCheckBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* ViewCubeCheckBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* WorldAxesCheckBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBox_FOV;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* PerspectiveRadioButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* OrthogonalRadioButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBox_Month;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBox_Day;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBox_Hour;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *EditableTextBox_Minute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButton_AM;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBox *EditableTextBox_AM;

	UFUNCTION()
	void OnEditableTextBoxFOVCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnEditableTextBoxMonthCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnEditableTextBoxDayCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnEditableTextBoxHourCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnEditableTextBoxMinuteCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnReleaseButtonModumateButtonAM();

	UFUNCTION()
	void ToggleGravityCheckboxes(bool NewEnable);

	UFUNCTION()
	void ToggleViewCubeCheckboxes(bool NewEnable);

	UFUNCTION()
	void ToggleWorldAxesCheckboxes(bool NewEnable);

	UFUNCTION()
	void ChangePerspectiveButton(bool bNewEnable);

	UFUNCTION()
	void ChangeOrthogonalButton(bool bNewEnable);

	void SyncTextBoxesWithSkyActorCurrentTime();
	void SyncAllMenuProperties();
};
