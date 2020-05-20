// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime);

public:
	UPROPERTY()
	int32 TargetObjID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder *RootBorder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox *RootVerticalBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox *ControlGabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox *ControlFlat;

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

protected:
	UFUNCTION()
	void OnControlChangedGabled(bool bIsChecked);

	UFUNCTION()
	void OnControlChangedFlat(bool bIsChecked);
};
