// Copyright 2018 Modumate, Inc. All Rights Reserved.
#include "UI/EditModelPlayerHUD.h"

#include "Components/Image.h"
#include "UI/WidgetClassAssetData.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ThumbnailCacheManager.h"


FAffordanceLine::FAffordanceLine(const FVector &InStartPoint, const FVector &InEndPoint,
	const FLinearColor &InColor, float InInterval, float InThickness, int32 InDrawPriority)
	: StartPoint(InStartPoint)
	, EndPoint(InEndPoint)
	, Color(InColor)
	, Interval(InInterval)
	, Thickness(InThickness)
	, DrawPriority(InDrawPriority)
{
}

AEditModelPlayerHUD::AEditModelPlayerHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WorldViewportWidgetTag(TEXT("WorldViewportWidget"))
{
}

void AEditModelPlayerHUD::Initialize()
{
	auto *controller = Cast<AEditModelPlayerController>(PlayerOwner);

	if (!ensureAlways(controller && WidgetClasses &&
		WidgetClasses->HUDDrawWidgetClass))
	{
		return;
	}

	HUDDrawWidget = CreateWidget<UHUDDrawWidget>(controller, WidgetClasses->HUDDrawWidgetClass);
	if (HUDDrawWidget != nullptr)
	{
		HUDDrawWidget->AddToViewport();
		HUDDrawWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void AEditModelPlayerHUD::DrawHUD()
{
	Super::DrawHUD();
}

bool AEditModelPlayerHUD::StaticCameraViewScreenshot(const FVector2D &ViewportSize, AEditModelPlayerController *EMPlayerController, UImage *ImageUI)
{
	if (!EMPlayerController)
	{
		return false;
	}

	// Try CopyViewportToTexture first, if failed, try to create a new texture and use it for copying to viewport
	// Reasons for failure: Texture size, format, nullptr
	bool bCopyViewport = UThumbnailCacheManager::CopyViewportToTexture(EMPlayerController->StaticCamTexture, EMPlayerController);
	if (!bCopyViewport)
	{
		// Create new texture
		float timeSec = EMPlayerController->GetWorld()->GetRealTimeSeconds();
		FString newTextureNameString = FString(TEXT("StaticCameraView")) + FString::SanitizeFloat(timeSec);		
		EMPlayerController->StaticCamTexture = UThumbnailCacheManager::CreateTexture2D(
			ViewportSize.X,
			ViewportSize.Y,
			1,
			PF_B8G8R8A8,
			EMPlayerController,
			FName(*newTextureNameString));
		
		// Reattempt with new texture
		bCopyViewport = UThumbnailCacheManager::CopyViewportToTexture(EMPlayerController->StaticCamTexture, EMPlayerController);
		if (!bCopyViewport)
		{
			UE_LOG(LogTemp, Error, TEXT("CopyViewportToTexture Failed"));
		}
	}

	// Apply texture to image
	if (bCopyViewport && ImageUI)
	{
		UMaterialInstanceDynamic* imageMat = ImageUI->GetDynamicMaterial();
		if (imageMat)
		{
			imageMat->SetTextureParameterValue(FName(TEXT("Texture")), EMPlayerController->StaticCamTexture);
			ImageUI->SetColorAndOpacity(FColor(255, 255, 255, 255));

			return true;
		}
	}

	return false;
}
