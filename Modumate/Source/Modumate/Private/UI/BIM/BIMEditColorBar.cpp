// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMEditColorBar.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/BIM/BIMEditColorPicker.h"
#include "UI/BIM/BIMDesigner.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Image.h"
#include "Kismet/KismetMathLibrary.h"

UBIMEditColorBar::UBIMEditColorBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMEditColorBar::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	return true;
}

void UBIMEditColorBar::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMEditColorBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// On Mac, IsInputKeyDown will be false for an extra frame after OnMouseButtonDown
	if (MouseClickCountdown > 0)
	{
		if (Controller->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			DragTick = true;
			MouseClickCountdown=0;
		}
		else
		{
			--MouseClickCountdown;
		}
	}
	if (DragTick)
	{
		PerformDrag();
	}
}

FReply UBIMEditColorBar::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// MouseClick delay for Mac...if it takes more than 10 frames, abort
		if (Controller->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			DragTick=true;
		}
		else
		{
			MouseClickCountdown=10;
		}
	}
	return reply;
}

void UBIMEditColorBar::PerformDrag()
{
	if (!(Controller && ParentColorPicker && ParentColorPicker->ParentBIMDesigner))
	{
		return;
	}

	float mouseX = 0.f;
	float mouseY = 0.f;
	UWidgetLayoutLibrary::GetMousePositionScaledByDPI(Controller, mouseX, mouseY);
	FVector2D pixelPos, viewportPos;
	USlateBlueprintLibrary::LocalToViewport(Controller, GetCachedGeometry(), FVector2D::ZeroVector, pixelPos, viewportPos);
	FVector2D currentMousePosition = FVector2D(mouseX, mouseY) - viewportPos;
	float scaledMouseValue = currentMousePosition.Y / ParentColorPicker->ParentBIMDesigner->GetCurrentZoomScale();
	float verticalSize = USlateBlueprintLibrary::GetLocalSize(GetCachedGeometry()).Y;

	float currentHueValue = (1.f - (scaledMouseValue / verticalSize)) * 360.f;

	FVector2D barOffset = FVector2D(0.f, FMath::Clamp(scaledMouseValue, 0.f, verticalSize));
	ColorLevelBarLeft->SetRenderTranslation(barOffset);
	ColorLevelBarRight->SetRenderTranslation(barOffset);

	DragTick = Controller->IsInputKeyDown(EKeys::LeftMouseButton);
	if (DragTick)
	{
		// Perform update on parent color picker
		ParentColorPicker->EditColorFromHue(currentHueValue);
	}
	else
	{
		// Perform update on BIMDesigner
		ParentColorPicker->UpdateParentNodeProperty();
	}
}

void UBIMEditColorBar::BuildColorBar(const FLinearColor& InHSV, class UBIMEditColorPicker* InParentColorPicker)
{
	ParentColorPicker = InParentColorPicker;

	float posY = (1.f - (InHSV.R / 360.f)) * ColorBarSize;
	FVector2D barOffset = FVector2D(0.f, FMath::Clamp(posY, 0.f, ColorBarSize));
	ColorLevelBarLeft->SetRenderTranslation(barOffset);
	ColorLevelBarRight->SetRenderTranslation(barOffset);
}
