// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/RightMenu/ComponentSavedViewListItem.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerPawn_CPP.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/Image.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UI/RightMenu/ViewMenuBlockSavedViews.h"
#include "UI/RightMenu/ViewMenuBlockProperties.h"
#include "UnrealClasses/SkyActor.h"


UViewMenuWidget::UViewMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UViewMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UViewMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (Controller)
	{
		PlayerPawn = Cast<AEditModelPlayerPawn_CPP>(Controller->GetPawn());
	}
	PreviewRT = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), PreviewRenderSize.X, PreviewRenderSize.Y, ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor::Black, true);
}

void UViewMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (EnableHoverCapture)
	{
		HoverCaptureTick();
	}
}

void UViewMenuWidget::SetViewMenuVisibility(bool NewVisible)
{
	if (NewVisible)
	{
		this->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ViewMenu_Block_SavedViews->UpdateSavedViewsList();
		ViewMenu_Block_Properties->SyncTextBoxesWithSkyActorCurrentTime();
	}
	else
	{
		this->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UViewMenuWidget::MouseOnHoverView(UComponentSavedViewListItem *Item)
{
	CurrentHoverViewItem = Item;
	BorderPreview->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	float mouseX, mouseY;
	if (Controller && Controller->GetMousePosition(mouseX, mouseY))
	{
		UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Controller->EditModelUserWidget->ViewMenu->BorderPreview);
		if (canvasSlot)
		{
			canvasSlot->SetPosition(FVector2D(canvasSlot->GetPosition().X, mouseY));
		}
	}
	// Tells HUD to save screenshot on next frame draw
	// TODO: Move EventReceiveDrawHUD logic from bpp to cpp
	Controller->GetEditModelHUD()->RequestStaticCameraView = true;
	EnableHoverCapture = true;
	HoverCaptureTickCount = 0;
}

void UViewMenuWidget::MouseEndHoverView(UComponentSavedViewListItem *Item)
{
	if (CurrentHoverViewItem == Item)
	{
		CurrentHoverViewItem = nullptr;
		BorderPreview->SetVisibility(ESlateVisibility::Collapsed);
		ImageBG->SetVisibility(ESlateVisibility::Collapsed);

		EnableHoverCapture = false;
		HoverCaptureTickCount = 0;
	}
	// Revert skyActor back to its original param
	Controller->SkyActor->UpdateComponentsWithDateTime(Controller->SkyActor->GetCurrentDateTime());
}

void UViewMenuWidget::HoverCaptureTick()
{
	if (Controller && !Controller->GetEditModelHUD()->RequestStaticCameraView)
	{
		HoverCaptureTickCount += 1;

		// The background image that captures the original scene appears right after the scene has been captured
		if (HoverCaptureTickCount == 1)
		{
			ImageBG->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

			PlayerPawn->CameraCaptureComponent2D->SetWorldLocationAndRotation(CurrentHoverViewItem->CameraView.Position, CurrentHoverViewItem->CameraView.Rotation);
			PlayerPawn->CameraCaptureComponent2D->FOVAngle = CurrentHoverViewItem->CameraView.FOV;
			PlayerPawn->CameraCaptureComponent2D->TextureTarget = PreviewRT;
			// Set lighting param to match with saved camera view
			Controller->SkyActor->UpdateComponentsWithDateTime(CurrentHoverViewItem->CameraView.TimeOfDay);
		}

		// Due to post-processing (ex: Light Propagation responding to time of day change), 
		// the scene is continually captured until the new lighting condition is sufficient
		// Change NumberOfTickForHoverPreview for number of tick needed before capturing ends
		if (HoverCaptureTickCount < NumberOfTickForHoverPreview)
		{
			PlayerPawn->CameraCaptureComponent2D->CaptureScene();
			static const FName textureParamName(TEXT("Texture"));
			ImagePreview->GetDynamicMaterial()->SetTextureParameterValue(textureParamName, PreviewRT);
		}
		else
		{
			EnableHoverCapture = false;
		}
	}
}
