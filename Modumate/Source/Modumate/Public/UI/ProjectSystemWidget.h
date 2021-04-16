// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "ProjectSystemWidget.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UProjectSystemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* ButtonBox;

protected:
	UFUNCTION()
	void OnPressedButton();
};
