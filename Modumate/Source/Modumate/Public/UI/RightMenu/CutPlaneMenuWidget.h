// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CutPlaneMenuWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UCutPlaneMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCutPlaneMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuBlock *CutPlaneMenuBlockHorizontal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuBlock *CutPlaneMenuBlockVertical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuBlock *CutPlaneMenuBlockOther;

	void SetViewMenuVisibility(bool NewVisible);
	void UpdateCutPlaneMenuBlocks();
};
