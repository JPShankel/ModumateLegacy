// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PresetCardMain.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardMain : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardMain(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton * MainButton;

	UFUNCTION()
	void OnMainButtonReleased();
};
