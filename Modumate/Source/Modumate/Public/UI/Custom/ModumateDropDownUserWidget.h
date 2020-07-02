// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ModumateDropDownUserWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateDropDownUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateDropDownUserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UComboBoxString *ComboBoxStringJustification;

protected:

};
