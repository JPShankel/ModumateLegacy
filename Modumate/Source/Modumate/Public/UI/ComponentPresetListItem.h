// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "Objects/ModumateObjectEnums.h"

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
	class UModumateTextBlockUserWidget* MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* GrabHandleImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImageBackground;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TextNumber;

	UPROPERTY()
	class UMaterialInterface* IconMaterial;

	bool CaptureIconFromPresetKey(class AEditModelPlayerController* Controller, const FGuid& InGUID);
	bool CaptureIconForBIMDesignerSwap(class AEditModelPlayerController* Controller, const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID);
};
