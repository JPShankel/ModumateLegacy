// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMBlockSlotListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockSlotListItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockSlotListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextBlockWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UButton* ButtonSlot;

	int32 SlotID = INDEX_NONE;
};
