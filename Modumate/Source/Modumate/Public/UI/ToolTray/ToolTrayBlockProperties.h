// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ToolTrayBlockProperties.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolTrayBlockProperties : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockProperties(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

};
