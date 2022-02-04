// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Quantities/QuantitiesCollection.h"
#include "ModumateCore/ModumateUnits.h"
#include "Objects/ModumateObjectEnums.h"

#include "PresetCardMetaDimension.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardMetaDimension : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardMetaDimension(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:
	void BuildAsMetaDimension(EObjectType ObjectType, double InDimensionValue);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* MetaObjectType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* DimensionValue;
};
