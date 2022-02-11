// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetCycle.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropBasisChanged, int32, BasisValue);

UCLASS()
class MODUMATE_API UInstPropWidgetCycle : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* CycleBasisButton;

	UPROPERTY()
	FOnInstPropBasisChanged ValueChangedEvent;

	void RegisterValue(UObject* Source);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnClickedCycleBtn();

	int32 LastBasisValue = 0;

};
