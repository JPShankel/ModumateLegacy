// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMBlockUserEnterable.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockUserEnterable : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockUserEnterable(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *Text_Title;
};
