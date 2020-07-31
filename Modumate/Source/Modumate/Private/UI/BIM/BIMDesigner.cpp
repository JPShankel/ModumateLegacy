// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMDesigner.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/CanvasPanel.h"
#include "UI/BIM/BIMBlockNode.h"

UBIMDesigner::UBIMDesigner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMDesigner::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

	return true;
}

void UBIMDesigner::NativeConstruct()
{
	Super::NativeConstruct();

	// For testing pan, zoom, and auto-arrange on nodes that already exist on start
	for (auto& curChildWidget : CanvasPanelForNodes->GetAllChildren())
	{
		UBIMBlockNode *curNode = Cast<UBIMBlockNode>(curChildWidget);
		if (curNode)
		{
			curNode->BIMDesigner = this;
			BIMBlockNodes.Add(curNode);
		}
	}
}

void UBIMDesigner::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DragTick)
	{
		PerformDrag();
	}
}

FReply UBIMDesigner::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == DragKey)
	{
		DragTick = true;
	}
	return reply;
}

FReply UBIMDesigner::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
	if (!canvasSlot)
	{
		return reply;
	}
	
	// Get the mouse position relative to widget
	FVector2D localMousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) - canvasSlot->GetPosition();
	FVector2D localMousePosScaled = localMousePos / GetCurrentZoomScale();

	// Predict the mouse position after the scaling is applied 
	float pendingZoomDelta = InMouseEvent.GetWheelDelta() > 0.f ? ZoomDeltaPlus : ZoomDeltaMinus;
	FVector2D localMousePosPostZoom = localMousePos / (GetCurrentZoomScale() / pendingZoomDelta);

	// Apply offset as if location under mouse never moved due to scaling
	FVector2D localZoomOffset = (localMousePosScaled - localMousePosPostZoom) * GetCurrentZoomScale();
	canvasSlot->SetPosition(canvasSlot->GetPosition() + localZoomOffset);

	ScaleBoxForNodes->SetUserSpecifiedScale(GetCurrentZoomScale() * pendingZoomDelta);
	return reply;
}

void UBIMDesigner::PerformDrag()
{
	float mouseX = 0.f;
	float mouseY = 0.f;
	bool hasMousePosition = Controller->GetMousePosition(mouseX, mouseY);
	FVector2D currentMousePosition = FVector2D(mouseX, mouseY) / UWidgetLayoutLibrary::GetViewportScale(this);

	if (hasMousePosition && !DragReset)
	{
		UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
		if (canvasSlot)
		{
			// Offset node by mouse delta
			FVector2D newCanvasPosition = (currentMousePosition - LastMousePosition) + canvasSlot->GetPosition();
			canvasSlot->SetPosition(newCanvasPosition);
		}

	}
	DragTick = Controller->IsInputKeyDown(DragKey) && hasMousePosition;
	if (DragTick)
	{
		LastMousePosition = currentMousePosition;
		DragReset = false;
	}
	else
	{
		DragReset = true;
	}

}

float UBIMDesigner::GetCurrentZoomScale()
{
	return ScaleBoxForNodes->UserSpecifiedScale;
}
