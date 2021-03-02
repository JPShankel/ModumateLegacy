// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PresetCardPropertyList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardPropertyList : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardPropertyList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

};
