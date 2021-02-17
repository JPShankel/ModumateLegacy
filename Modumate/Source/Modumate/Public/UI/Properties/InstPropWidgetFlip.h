// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetFlip.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropFlipChanged, int32, FlippedAxisInt);

UCLASS()
class MODUMATE_API UInstPropWidgetFlip : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonFlipX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonFlipY;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonFlipZ;

	EAxisList::Type EnabledAxes = EAxisList::None;

	UPROPERTY()
	FOnInstPropFlipChanged ValueChangedEvent;

	void RegisterValue(UObject* Source, EAxisList::Type RequestedAxes);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnClickedFlipX();

	UFUNCTION()
	void OnClickedFlipY();

	UFUNCTION()
	void OnClickedFlipZ();

	EAxis::Type LastClickedAxis = EAxis::None;
};
