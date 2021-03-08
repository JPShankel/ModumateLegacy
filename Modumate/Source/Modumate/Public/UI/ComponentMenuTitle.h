// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "ComponentMenuTitle.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UComponentMenuTitle : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* MenuTitle;
};
