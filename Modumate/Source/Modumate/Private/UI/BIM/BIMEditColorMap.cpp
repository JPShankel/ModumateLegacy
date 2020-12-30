// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMEditColorMap.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/BIM/BIMEditColorPicker.h"
#include "UI/BIM/BIMDesigner.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Image.h"
#include "Kismet/KismetMathLibrary.h"

UBIMEditColorMap::UBIMEditColorMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMEditColorMap::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

	return true;
}

void UBIMEditColorMap::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMEditColorMap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DragTick)
	{
		PerformDrag();
	}
}

FReply UBIMEditColorMap::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		DragTick = true;
	}
	return reply;
}

void UBIMEditColorMap::PerformDrag()
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
	FVector2D scaledMouseValue = currentMousePosition / ParentColorPicker->ParentBIMDesigner->GetCurrentZoomScale();
	FVector2D mapSize = USlateBlueprintLibrary::GetLocalSize(GetCachedGeometry());

	FVector2D clampedVector = (scaledMouseValue / mapSize).ClampAxes(0.f, 1.f);
	float currentSaturationValue = clampedVector.X;
	float currentBrightnessValue = 1.f - clampedVector.Y;
	PickerIcon->SetRenderTranslation(FVector2D(FMath::Clamp(scaledMouseValue.X, 0.f, mapSize.X), FMath::Clamp(scaledMouseValue.Y, 0.f, mapSize.Y)));

	DragTick = Controller->IsInputKeyDown(EKeys::LeftMouseButton);
	if (DragTick)
	{
		// Perform update on parent color picker
		ParentColorPicker->EditColorFromSaturationAndBrightness(currentSaturationValue, currentBrightnessValue);
	}
	else
	{
		// Perform update on BIMDesigner
		ParentColorPicker->UpdateParentNodeProperty();
	}
}

void UBIMEditColorMap::BuildColorMap(const FLinearColor& InHSV, class UBIMEditColorPicker* InParentColorPicker)
{
	ParentColorPicker = InParentColorPicker;

	UpdateColorMapGradient(InHSV.R);
	float posX = InHSV.G * ColorGradientMapSize.X;
	float posY = (1.f - InHSV.B) * ColorGradientMapSize.Y;
	PickerIcon->SetRenderTranslation(FVector2D(posX, posY));
}

void UBIMEditColorMap::UpdateColorMapGradient(float InHue)
{
	UMaterialInstanceDynamic* dynMat = ColorGradientMap->GetDynamicMaterial();
	if (dynMat)
	{
		FLinearColor inColor = UKismetMathLibrary::HSVToRGB(InHue, 1.f, 1.f, 1.f);
		dynMat->SetVectorParameterValue(ColorGradientMapHueParamName, inColor);
	}
}
