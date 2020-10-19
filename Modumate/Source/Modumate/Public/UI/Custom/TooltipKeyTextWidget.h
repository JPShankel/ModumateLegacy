// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "TooltipKeyTextWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UTooltipKeyTextWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTooltipKeyTextWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* ModumateTextBlock;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextOverride;

	void ChangeText(const FText &NewText);
};
