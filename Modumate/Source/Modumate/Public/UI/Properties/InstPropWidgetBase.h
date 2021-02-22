// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "InstPropWidgetBase.generated.h"


UCLASS()
class MODUMATE_API UInstPropWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp();
	virtual void DisplayValue() PURE_VIRTUAL(UInstPropWidgetBase::DisplayValue);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* PropertyTitle;

	bool IsInstPropInitialized() const;
	bool IsRegistered() const;
	int32 GetNumRegistrations() const;
	bool IsValueConsistent() const;

	void SetPropertyTitle(const FText& TitleText);

	static const FText& GetMixedDisplayText();

protected:
	// TODO: it seems like it should be possible to remove this generalized delegate-caller with some UObject/UFunction scripting/template magic.
	virtual void BroadcastValueChanged() PURE_VIRTUAL(UInstPropWidgetBase::BroadcastValueChanged);

	// This must be called after the widget has its value changed via its own fields, in order to propagate changes to interested systems.
	void PostValueChanged();

	// Keeps track of value consistency and registrations, to know whether the property should be visible or displayed as inconsistent.
	void OnRegisteredValue(UObject* Source, bool bValueEqualsCurrent);

	bool bInstPropInitialized = false;
	int32 NumRegistrations = 0;
	bool bConsistentValue = true;
};
