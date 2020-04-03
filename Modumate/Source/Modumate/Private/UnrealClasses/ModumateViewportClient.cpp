// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateViewportClient.h"

#include "Engine.h"
#include "Slate/SceneViewport.h"

void UModumateViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);

	SetCaptureMouseOnClick(EMouseCaptureMode::CaptureDuringRightMouseDown);
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
