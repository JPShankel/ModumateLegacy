// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Quantities/QuantitiesCollection.h"

#include "PresetCardQuantityListTotal.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardQuantityListTotal : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardQuantityListTotal(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* FieldTitleMeasurmentType1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* FieldTitleMeasurmentType2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Quantity1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Quantity2;

	void BuildTotalLabel(const FQuantity& InQuantity);
};
