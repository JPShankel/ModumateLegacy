// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PresetCardObjectList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardObjectList : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardObjectList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

};
