// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMScopeWarningListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMScopeWarningListItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMScopeWarningListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* PresetTitle;

	void BuildItem(const FString& ItemName);
};
