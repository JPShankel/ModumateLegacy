// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CutPlaneMenuBlock.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UCutPlaneMenuBlock : public UUserWidget
{
	GENERATED_BODY()

public:
	UCutPlaneMenuBlock(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView *CutPlanesList;
};
