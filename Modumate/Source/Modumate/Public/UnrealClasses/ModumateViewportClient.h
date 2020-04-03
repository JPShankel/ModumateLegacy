// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "ModumateViewportClient.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

	//~ Begin UGameViewportClient Interface.
	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	//~ End UGameViewportClient Interface.

	//~ Begin FViewportClient Interface.
	virtual void LostFocus(FViewport* Viewport) override;
	virtual void ReceivedFocus(FViewport* Viewport) override;
	//~ End FViewportClient Interface.
};
