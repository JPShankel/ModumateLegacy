// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetGableControl.generated.h"


UCLASS()
class MODUMATE_API UInstPropWidgetGableControl : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override { return Super::Initialize(); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxGabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateCheckBox* CheckBoxHasFace;

protected:
	UFUNCTION()
	void OnCheckBoxGabledChanged(bool bIsChecked) { }

	UFUNCTION()
	void OnCheckBoxHasFaceChanged(bool bIsChecked) { }
};
