// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMKey.h"
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

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *IconImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *GrabHandleImage;

	bool CaptureIconFromPresetKey(class AEditModelPlayerController_CPP *Controller, const FBIMKey& AsmKey, EToolMode mode);
	bool CaptureIconForBIMDesignerSwap(class AEditModelPlayerController_CPP *Controller, const FBIMKey& PresetKey, int32 NodeID);
};
