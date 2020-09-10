// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "ModumateCore/ModumateTypes.h"

#include "CutPlaneDimListItem.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class ECutPlaneType : uint8
{
	Horizontal,
	Vertical,
	Other,
	None
};

UCLASS()
class MODUMATE_API UCutPlaneDimListItem : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UCutPlaneDimListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	int32 ObjID = MOD_ID_NONE;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCheckBox *CheckBoxVisibility;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *TextDimension;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *TextTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButtonMain;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSave;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonEdit;

	UFUNCTION()
	void OnButtonMainReleased();

	UFUNCTION()
	void OnButtonEditReleased();

	UFUNCTION()
	void OnButtonSaveReleased();

	UFUNCTION()
	void OnCheckBoxVisibilityChanged(bool IsChecked);

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	void BuildAsVerticalCutPlaneItem(const FQuat &Rotation);
	void BuildAsHorizontalCutPlaneItem(const FVector &Location);
	void UpdateCheckBoxVisibility(bool NewVisible);
};
