// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMKey.h"
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
	class AEditModelPlayerController_CPP *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *Button_AddLayer;

	int32 ParentID = -1;
	FBIMKey PresetID;
	int32 ParentSetIndex = -1;
	int32 ParentSetPosition = -1;

	UFUNCTION()
	void OnButtonAddReleased();
};
