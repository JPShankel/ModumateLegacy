// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Graph/GraphCommon.h"
#include "Objects/ModumateRoofStatics.h"

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

	void SetTarget(int32 InTargetPerimeterID, FGraphSignedID InTargetEdgeID, class AAdjustmentHandleActor *InOwningHandle);
	void UpdateEditFields();
	void UpdateTransform();

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY()
	int32 TargetPerimeterID;

	UPROPERTY()
	int32 TargetEdgeID;

	UPROPERTY()
	class AAdjustmentHandleActor *OwningHandle;

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
	FMOIRoofPerimeterData TempInitialCombinedProperties;

	UPROPERTY()
	class AEditModelGameState *GameState;

	UFUNCTION()
	FEventReply OnClickedBorder(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

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
	void OnOverhangEditorTextChanged(const FText& NewText);

	UFUNCTION()
	void OnOverhangEditorTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnButtonPressedCommit();

	UFUNCTION()
	void OnButtonPressedCancel();

	void SetEdgeFaceControl(bool bNewHasFace);
	void SetEdgeSlope(float SlopeValue);
	void SaveEdgeProperties();
};
