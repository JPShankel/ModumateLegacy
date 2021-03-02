// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PresetCardPropertySubList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardPropertySubList : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardPropertySubList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

};
