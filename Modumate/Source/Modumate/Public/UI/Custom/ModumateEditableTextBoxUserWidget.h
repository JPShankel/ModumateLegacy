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

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EllipsizeWordAt = 18;

	void ChangeText(const FText &NewText, bool EllipsizeText = true);

protected:

};
