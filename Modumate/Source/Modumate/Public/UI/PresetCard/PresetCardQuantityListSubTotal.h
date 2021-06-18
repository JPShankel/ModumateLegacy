// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Quantities/QuantitiesCollection.h"

#include "PresetCardQuantityListSubTotal.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardQuantityListSubTotal : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardQuantityListSubTotal(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TributaryType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* FieldTitleMeasurmentType1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* FieldTitleMeasurmentType2;

	void BuildSubLabel(const FText& LabelText, const FQuantity& InQuantity, bool bMetric);
};
