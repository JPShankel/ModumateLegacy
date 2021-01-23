// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RightMenu/CutPlaneDimListItem.h"

#include "CutPlaneMenuBlockExport.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UCutPlaneMenuBlockExport : public UUserWidget
{
	GENERATED_BODY()

public:
	UCutPlaneMenuBlockExport(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* ListHorizontal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* ListVertical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* ListOther;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonExport;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonCancel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxAll;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxHorizontal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxVertical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxOther;

	UFUNCTION()
	void OnCheckBoxHorizontalChanged(bool IsChecked);

	UFUNCTION()
	void OnCheckBoxVerticalChanged(bool IsChecked);

	UFUNCTION()
	void OnCheckBoxOtherChanged(bool IsChecked);

	UFUNCTION()
	void OnCheckBoxAllChanged(bool IsChecked);

	UFUNCTION()
	void OnButtonExportReleased();

	UFUNCTION()
	void OnButtonCancelReleased();

	void SetExportMenuVisibility(bool NewVisible);
	void UpdateExportMenu();
	void UpdateCategoryCheckboxes();

	void UpdateCutPlaneExportable(const TArray<int32>& IDs, bool NewExportable);
	void UpdateCutPlaneExportableByType(ECutPlaneType Type, bool NewExportable);
};
