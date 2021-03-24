// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Quantities/QuantitiesCollection.h"

#include "PresetCardQuantityListSubTotalListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardQuantityListSubTotalListItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardQuantityListSubTotalListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* AssemblyTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Quantity1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Quantity2;

	void BuildAsSubTotalListItem(const FQuantityItemId& QuantityItemID, const FQuantity& InQuantity);
};
