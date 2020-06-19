// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/AdjustmentHandleWidget.h"

#include "Components/Button.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Widgets/SWidget.h"


const FName UAdjustmentHandleWidget::SlateTag(TEXT("AdjustmentHandle"));

UAdjustmentHandleWidget::UAdjustmentHandleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DesiredSize(16.0f, 16.0f)
	, MainButtonOffset(ForceInitToZero)
	, bIsButtonHovered(false)
{

}

bool UAdjustmentHandleWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (MainButton == nullptr)
	{
		return false;
	}

	MainButton->OnPressed.AddDynamic(this, &UAdjustmentHandleWidget::OnPressed);
	MainButton->OnHovered.AddDynamic(this, &UAdjustmentHandleWidget::OnHovered);
	MainButton->OnUnhovered.AddDynamic(this, &UAdjustmentHandleWidget::OnUnhovered);

	return true;
}

void UAdjustmentHandleWidget::SetMainButtonStyle(const USlateWidgetStyleAsset *ButtonStyleAsset)
{
	const FButtonStyle *buttonStyle = ButtonStyleAsset ? ButtonStyleAsset->GetStyle<FButtonStyle>() : nullptr;
	if (buttonStyle)
	{
		MainButton->SetStyle(*buttonStyle);
	}
}

void UAdjustmentHandleWidget::UpdateTransform()
{
	if (HandleActor)
	{
		auto *controller = GetOwningPlayer();
		FVector targetWorldPos = HandleActor->GetHandlePosition();
		FVector targetWorldDir = HandleActor->GetHandleDirection();
		FVector2D targetScreenPos;
		if (controller && controller->ProjectWorldLocationToScreen(targetWorldPos, targetScreenPos))
		{
			float rotationAngle = 0.0f;
			FVector2D targetOffsetPos;
			if (targetWorldDir.IsNormalized() &&
				controller->ProjectWorldLocationToScreen(targetWorldPos + targetWorldDir, targetOffsetPos) &&
				!targetOffsetPos.Equals(targetScreenPos))
			{
				FVector2D offsetDelta = targetOffsetPos - targetScreenPos;
				rotationAngle = FMath::RadiansToDegrees(FMath::Atan2(offsetDelta.Y, offsetDelta.X));
			}

			FVector2D widgetSize = GetCachedGeometry().GetAbsoluteSize();
			SetPositionInViewport(targetScreenPos);
			SetRenderTranslation(-0.5f * DesiredSize);
			SetRenderTransformAngle(rotationAngle);
			SetDesiredSizeInViewport(DesiredSize);
			MainButton->SetRenderTranslation(MainButtonOffset);
			SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UAdjustmentHandleWidget::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	TSharedRef<SWidget> slateWidget = TakeWidget();
	slateWidget->SetTag(SlateTag);
}

void UAdjustmentHandleWidget::OnHoverChanged(bool bNewHovered)
{
	if (bIsButtonHovered != bNewHovered)
	{
		bIsButtonHovered = bNewHovered;

		auto *controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		if (controller)
		{
			controller->OnHoverHandleWidget(this, bNewHovered);
		}
	}
}

void UAdjustmentHandleWidget::OnPressed()
{
	auto *controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller)
	{
		controller->OnPressHandleWidget(this, true);
	}
}

void UAdjustmentHandleWidget::OnHovered()
{
	OnHoverChanged(true);
}

void UAdjustmentHandleWidget::OnUnhovered()
{
	OnHoverChanged(false);
}
