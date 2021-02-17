// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetRotation.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropRotationSet, float, NewValue);

UCLASS()
class MODUMATE_API UInstPropWidgetRotation : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* RotationText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UWidget* RotationTextSuffix;

	UPROPERTY()
	float CurrentValue;

	UPROPERTY()
	FOnInstPropRotationSet ValueChangedEvent;

	UFUNCTION()
	void RegisterValue(UObject* Source, float RotationValue);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnRotationTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod);
};
