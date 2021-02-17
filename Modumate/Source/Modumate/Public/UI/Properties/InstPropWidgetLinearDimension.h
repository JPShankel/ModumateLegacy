// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetLinearDimension.generated.h"


UCLASS()
class MODUMATE_API UInstPropWidgetLinearDimension : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override { return Super::Initialize(); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* DimensionText;

protected:
	UFUNCTION()
	void OnDimensionTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { }
};
