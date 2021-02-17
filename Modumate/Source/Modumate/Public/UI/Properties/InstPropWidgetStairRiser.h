// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetStairRiser.generated.h"


UCLASS()
class MODUMATE_API UInstPropWidgetStairRiser : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override { return Super::Initialize(); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxStart;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxEnd;

protected:
	UFUNCTION()
	void OnCheckBoxStartChanged(bool bIsChecked) { }

	UFUNCTION()
	void OnCheckBoxEndChanged(bool bIsChecked) { }
};
