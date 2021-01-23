// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/ModumateViewportClient.h"

#include "Engine.h"
#include "Slate/SceneViewport.h"

void UModumateViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);

	SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
	SetHideCursorDuringCapture(true);

	bWindowsActive = true;
	bWindowsVisible = true;
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
	CheckWindows();
}

void UModumateViewportClient::ReceivedFocus(FViewport* InViewport)
{
	Super::ReceivedFocus(InViewport);
	CheckWindows();
}

void UModumateViewportClient::Activated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	Super::Activated(InViewport, InActivateEvent);
	CheckWindows();
}

void UModumateViewportClient::Deactivated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	Super::Deactivated(InViewport, InActivateEvent);
	CheckWindows();
}

void UModumateViewportClient::CheckWindows()
{
	bool bNewWindowsActive = false;
	bool bNewWindowsVisible = false;

	auto windows = FSlateApplication::Get().GetInteractiveTopLevelWindows();

	for (const TSharedRef<SWindow>& window : windows)
	{
		if (window->IsActive())
		{
			bNewWindowsActive = true;
		}

		if (!window->IsWindowMinimized() && window->IsVisible())
		{
			bNewWindowsVisible = true;
		}
	}

	if (bWindowsActive != bNewWindowsActive)
	{
		bWindowsActive = bNewWindowsActive;
	}

	if (bWindowsVisible != bNewWindowsVisible)
	{
		bWindowsVisible = bNewWindowsVisible;
		FViewport::SetGameRenderingEnabled(bWindowsVisible);
		GEngine->OnLostFocusPause(!bWindowsVisible);
	}
}
