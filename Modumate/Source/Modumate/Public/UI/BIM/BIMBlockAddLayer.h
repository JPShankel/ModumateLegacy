// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "BIMBlockAddLayer.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMBlockAddLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockAddLayer(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class UBIMDesigner *ParentBIMDesigner;

	UPROPERTY()
	class AEditModelPlayerController *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *Button_AddLayer;

	FBIMEditorNodeIDType ParentID;
	FGuid PresetID;
	int32 ParentSetIndex = INDEX_NONE;

	UFUNCTION()
	void OnButtonAddReleased();
};
