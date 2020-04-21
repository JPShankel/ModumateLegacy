// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "ModumateViewportClient.generated.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FOnMouseEnter, FIntPoint);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMouseLeave, FIntPoint);

UCLASS()
class MODUMATE_API UModumateViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	//~ Begin UGameViewportClient Interface.
	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void MouseEnter(FViewport* Viewport, int32 x, int32 y) override;
	virtual void MouseLeave(FViewport* Viewport) override;
	//~ End UGameViewportClient Interface.

	//~ Begin FViewportClient Interface.
	virtual void LostFocus(FViewport* Viewport) override;
	virtual void ReceivedFocus(FViewport* Viewport) override;
	//~ End FViewportClient Interface.

	FOnMouseEnter OnMouseEnterDelegate;
	FOnMouseLeave OnMouseLeaveDelegate;
};
