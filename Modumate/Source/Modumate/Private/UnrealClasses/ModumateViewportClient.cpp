// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateViewportClient.h"

#include "Engine.h"
#include "Slate/SceneViewport.h"

void UModumateViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);

	SetCaptureMouseOnClick(EMouseCaptureMode::CaptureDuringRightMouseDown);
	SetHideCursorDuringCapture(true);
}

void UModumateViewportClient::MouseEnter(FViewport* InViewport, int32 x, int32 y)
{
	Super::MouseEnter(InViewport, x, y);

	OnMouseEnterDelegate.Broadcast(FIntPoint(x, y));
}

void UModumateViewportClient::MouseLeave(FViewport* InViewport)
{
	Super::MouseLeave(InViewport);

	FVector2D absCursorPos = FSlateApplication::Get().GetCursorPos();
	FSceneViewport *sceneViewport = static_cast<FSceneViewport*>(InViewport);
	FVector2D localCursorPos = sceneViewport->GetCachedGeometry().AbsoluteToLocal(absCursorPos);

	OnMouseLeaveDelegate.Broadcast(localCursorPos.IntPoint());
}

void UModumateViewportClient::LostFocus(FViewport* InViewport)
{
	Super::LostFocus(InViewport);

	bool bAllHidden = true;
	auto windows = FSlateApplication::Get().GetInteractiveTopLevelWindows();

	for (const TSharedRef<SWindow>& window : windows)
	{
		if (!window->IsWindowMinimized() && window->IsVisible())
		{
			bAllHidden = false;
			break;
		}
	}

	if (bAllHidden)
	{
		FViewport::SetGameRenderingEnabled(false);
		GEngine->OnLostFocusPause(true);
	}
}

void UModumateViewportClient::ReceivedFocus(FViewport* InViewport)
{
	Super::ReceivedFocus(InViewport);

	FViewport::SetGameRenderingEnabled(true);
	GEngine->OnLostFocusPause(false);
}
