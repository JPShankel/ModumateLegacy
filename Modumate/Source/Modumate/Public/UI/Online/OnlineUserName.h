// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Custom/ModumateTextBlockUserWidget.h"

#include "OnlineUserName.generated.h"


UCLASS()
class MODUMATE_API UOnlineUserName : public UModumateTextBlockUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder* Background;
};
