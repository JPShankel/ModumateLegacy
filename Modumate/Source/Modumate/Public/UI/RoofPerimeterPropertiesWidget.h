// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModumateCore/ModumateRoofStatics.h"

#include "RoofPerimeterPropertiesWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API URoofPerimeterPropertiesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URoofPerimeterPropertiesWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	void SetTarget(int32 InTargetPerimeterID, int32 InTargetEdgeID, class AAdjustmentHandleActor_CPP *InOwningHandle);
	void UpdateTransform();

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY()
	int32 TargetPerimeterID;

	UPROPERTY()
	int32 TargetEdgeID;

	UPROPERTY()
	class AAdjustmentHandleActor_CPP *OwningHandle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder *RootBorder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox *RootVerticalBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox *ControlGabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox *ControlHasFace;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UEditableTextBox *SlopeEditorFraction;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UEditableTextBox *SlopeEditorDegrees;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UEditableTextBox *OverhangEditor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UButton *ButtonCommit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UButton *ButtonCancel;

	UPROPERTY(VisibleAnywhere)
	FRoofEdgeProperties InitialProperties;

	UPROPERTY(VisibleAnywhere)
	FRoofEdgeProperties CurrentProperties;

protected:
	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

	UFUNCTION()
	void OnControlChangedGabled(bool bIsChecked);

	UFUNCTION()
	void OnControlChangedHasFace(bool bIsChecked);

	UFUNCTION()
	void OnSlopeEditorFractionTextChanged(const FText& NewText);

	UFUNCTION()
	void OnSlopeEditorFractionTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnSlopeEditorDegreesTextChanged(const FText& NewText);

	UFUNCTION()
	void OnSlopeEditorDegreesTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnButtonPressedCommit();

	UFUNCTION()
	void OnButtonPressedCancel();

	void SetEdgeFaceControl(bool bNewHasFace);
	void SetEdgeSlope(float SlopeValue);
	void SaveEdgeProperties();
};
