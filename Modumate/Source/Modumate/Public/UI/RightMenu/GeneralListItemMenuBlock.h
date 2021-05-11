// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "GeneralListItemMenuBlock.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UGeneralListItemMenuBlock : public UUserWidget
{
	GENERATED_BODY()

public:
	UGeneralListItemMenuBlock(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* GeneralItemsList;
};
