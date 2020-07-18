// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"

#include "ComponentPresetListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UComponentPresetListItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UComponentPresetListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	class UTextureRenderTarget2D *IconRenderTarget;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *IconImage;

	bool CaptureIconFromPresetKey(class AEditModelPlayerController_CPP *Controller, const FName &AsmKey, EToolMode mode);
};
