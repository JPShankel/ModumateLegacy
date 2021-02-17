// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetStairTread.generated.h"


UCLASS()
class MODUMATE_API UInstPropWidgetStairTread : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override { return Super::Initialize(); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* MinDepthText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* MaxDepthText;

protected:
	UFUNCTION()
	void OnMinDepthTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { }

	UFUNCTION()
	void OnMaxDepthTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod) { }
};
