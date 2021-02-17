// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetSlope.generated.h"


UCLASS()
class MODUMATE_API UInstPropWidgetSlope : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override { return Super::Initialize(); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget *SlopeEditorFraction;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* SlopeEditorDegrees;

protected:
	UFUNCTION()
	void OnSlopeEditorFractionTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { }

	UFUNCTION()
	void OnSlopeEditorDegreesTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { }
};
