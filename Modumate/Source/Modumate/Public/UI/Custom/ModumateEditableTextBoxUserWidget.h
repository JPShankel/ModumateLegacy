// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ModumateEditableTextBoxUserWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateEditableTextBoxUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateEditableTextBoxUserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBox *ModumateEditableTextBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USizeBox *SizeBoxEditable;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText HintOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EllipsizeWordAt = 18;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_WidthOverride"))
	float WidthOverride = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_HeightOverride"))
	float HeightOverride = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_MinDesiredWidth"))
	float MinDesiredWidth = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_MinDesiredHeight"))
	float MinDesiredHeight = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_MaxDesiredWidth"))
	float MaxDesiredWidth = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_MaxDesiredHeight"))
	float MaxDesiredHeight = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_MinAspectRatio"))
	float MinAspectRatio = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (editcondition = "bOverride_MaxAspectRatio"))
	float MaxAspectRatio = 1.f;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_WidthOverride : 1;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_HeightOverride : 1;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_MinDesiredWidth : 1;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_MinDesiredHeight : 1;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_MaxDesiredWidth : 1;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_MaxDesiredHeight : 1;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_MinAspectRatio : 1;

	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	uint8 bOverride_MaxAspectRatio : 1;

	void ChangeText(const FText &NewText, bool EllipsizeText = true);
	void ChangeHint(const FText &NewHint, bool EllipsizeHint = true);
	void OverrideSizeBox();

protected:

};
