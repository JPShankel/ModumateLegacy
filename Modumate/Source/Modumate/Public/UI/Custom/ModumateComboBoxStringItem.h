// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ModumateComboBoxStringItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateComboBoxStringItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateComboBoxStringItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *ModumateTextBlockUserWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UImage *ImageBlock;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextOverride;

	void BuildItem(FText NewText, UTexture2D *NewTexture = nullptr);

};
