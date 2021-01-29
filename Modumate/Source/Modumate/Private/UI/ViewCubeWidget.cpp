#include "UI/ViewCubeWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelPlayerCameraManager.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"

UViewCubeWidget::UViewCubeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultCameraForward(FVector::ForwardVector)
{

}

bool UViewCubeWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = Cast<AEditModelPlayerController>(GetOwningPlayer());
	CameraManager = Controller ? Controller->PlayerCameraManager : nullptr;

	if (!(ButtonSnapUp && ButtonSnapDown && ButtonSnapLeft && ButtonSnapRight && ButtonSnapForward && ButtonSnapBackward))
	{
		return false;
	} 

	ButtonSnapUp->ModumateButton->OnReleased.AddDynamic(this, &UViewCubeWidget::OnButtonPressedSnapUp);
	ButtonSnapDown->ModumateButton->OnReleased.AddDynamic(this, &UViewCubeWidget::OnButtonPressedSnapDown);
	ButtonSnapLeft->ModumateButton->OnReleased.AddDynamic(this, &UViewCubeWidget::OnButtonPressedSnapLeft);
	ButtonSnapRight->ModumateButton->OnReleased.AddDynamic(this, &UViewCubeWidget::OnButtonPressedSnapRight);
	ButtonSnapForward->ModumateButton->OnReleased.AddDynamic(this, &UViewCubeWidget::OnButtonPressedSnapForward);
	ButtonSnapBackward->ModumateButton->OnReleased.AddDynamic(this, &UViewCubeWidget::OnButtonPressedSnapBackward);

	DirectionToButton.Add(FVector::UpVector, ButtonSnapUp);
	DirectionToButton.Add(FVector::DownVector, ButtonSnapDown);
	DirectionToButton.Add(FVector::LeftVector, ButtonSnapLeft);
	DirectionToButton.Add(FVector::RightVector, ButtonSnapRight);
	DirectionToButton.Add(FVector::ForwardVector, ButtonSnapForward);
	DirectionToButton.Add(FVector::BackwardVector, ButtonSnapBackward);

	return true;
}

int32 UViewCubeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 parentMaxLayerID = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	int32 selfPaintLayerID = LayerId;

	FPaintContext selfPaintContext(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FVector forwardVector = CameraManager ? CameraManager->GetActorForwardVector() : DefaultCameraForward;

	for (auto& kvp : DirectionToButton)
	{
		auto& direction = kvp.Key;
		auto* button = kvp.Value;

		// only show lines and buttons for faces with normals that would face the camera
		if ((forwardVector | direction) < THRESH_NORMALS_ARE_ORTHOGONAL)
		{
			button->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}
		button->SetVisibility(ESlateVisibility::Visible);

		FVector axisX, axisY;
		UModumateGeometryStatics::FindBasisVectors(axisX, axisY, direction);
		TArray<FVector> points = {
			direction + axisX + axisY,
			direction + axisX - axisY,
			direction - axisX - axisY,
			direction - axisX + axisY
		};

		FWidgetTransform newTransform;
		newTransform.Translation = TransformVector(direction * CubeSideLength);
		button->SetRenderTransform(newTransform);

		for (int32 i = 0; i < 4; i++)
		{
			FVector2D pointStart = TransformVector(CubeSideLength * points[i]);
			FVector2D pointEnd = TransformVector(CubeSideLength * points[(i + 1) % 4]);
			FVector2D lineDir = (pointEnd - pointStart).GetSafeNormal();
			pointStart += 0.5f * LineThickness * lineDir;
			pointEnd -= 0.5f * LineThickness * lineDir;

			UWidgetBlueprintLibrary::DrawLine(selfPaintContext,
				pointStart, pointEnd,
				LineColor, true, LineThickness);
		}
	}

	return FMath::Max(parentMaxLayerID, selfPaintLayerID);
}

void UViewCubeWidget::OnButtonPressedSnapUp()
{
	Zoom(FVector::UpVector, FVector::LeftVector);
}

void UViewCubeWidget::OnButtonPressedSnapDown()
{
	Zoom(FVector::DownVector, FVector::LeftVector);
}

void UViewCubeWidget::OnButtonPressedSnapLeft()
{
	Zoom(FVector::LeftVector, FVector::UpVector);
}

void UViewCubeWidget::OnButtonPressedSnapRight()
{
	Zoom(FVector::RightVector, FVector::UpVector);
}

void UViewCubeWidget::OnButtonPressedSnapForward()
{
	Zoom(FVector::ForwardVector, FVector::UpVector);
}

void UViewCubeWidget::OnButtonPressedSnapBackward()
{
	Zoom(FVector::BackwardVector, FVector::UpVector);
}

void UViewCubeWidget::Zoom(const FVector& InForward, const FVector& InUp)
{
	if (Controller == nullptr)
	{
		return;
	}

	if (Controller->EMPlayerState->SelectedObjects.Num() > 0)
	{
		Controller->CameraController->ZoomToSelection(InForward, InUp);
	}
	else
	{
		Controller->CameraController->ZoomToProjectExtents(InForward, InUp);
	}

	static const FString eventName(TEXT("QuickViewSet"));
	UModumateAnalyticsStatics::RecordEventSimple(this, UModumateAnalyticsStatics::EventCategoryView, eventName);
}

FVector2D UViewCubeWidget::TransformVector(const FVector& InVector) const
{
	FVector cameraRight, cameraUp;
	if (CameraManager)
	{
		cameraRight = CameraManager->GetActorRightVector();
		cameraUp = CameraManager->GetActorUpVector();
	}
	else
	{
		FQuat defaultCameraRot = FRotationMatrix::MakeFromX(DefaultCameraForward).ToQuat();
		cameraRight = defaultCameraRot.GetRightVector();
		cameraUp = defaultCameraRot.GetUpVector();
	}

	float x = (-cameraRight) | (InVector);
	float y = cameraUp | (InVector);
	return FVector2D(x, y);
}
