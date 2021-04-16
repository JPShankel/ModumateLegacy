#include "UnrealClasses/DimensionWidget.h"

#include "Components/EditableTextBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateUnits.h"
#include "UI/EditModelPlayerHUD.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UnrealClasses/EditModelPlayerController.h"

#define LOCTEXT_NAMESPACE "UDimensionWidget"

UDimensionWidget::UDimensionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsFocusable = true;
}

bool UDimensionWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (Measurement == nullptr)
	{
		return false;
	}

	Measurement->AllowContextMenu = false;

	ResetWidget();

	return true;
}

void UDimensionWidget::SetIsEditable(bool bIsEditable)
{
	Measurement->SetIsReadOnly(!bIsEditable);
	!bIsEditable ? Measurement->WidgetStyle.SetFont(ReadOnlyFont) : Measurement->WidgetStyle.SetFont(EditableFont);
}

void UDimensionWidget::UpdateLengthTransform(const FVector2D& Position, const FVector2D& EdgeDirection, const FVector2D& OffsetDirection, double Length)
{
	FVector2D widgetSize = GetCachedGeometry().GetAbsoluteSize();

	SetPositionInViewport(Position - ((PixelOffset + widgetSize.Y / 2.0f) * OffsetDirection));

	float angle = FMath::RadiansToDegrees(FMath::Atan2(EdgeDirection.Y, EdgeDirection.X));
	angle = FRotator::ClampAxis(angle);

	// flip the text if it is going to appear upside down
	if (angle > 90.0f && angle <= 270.0f)
	{
		angle -= 180.0f;
	}

	Measurement->SetRenderTransformAngle(angle);

	// Update text is called here to make sure that the text matches the current length
	// even if something else changed the length (ex. Undo)
	UpdateText(Length);
}

void UDimensionWidget::UpdateDegreeTransform(const FVector2D& Position, const FVector2D& OffsetDirection, double Angle)
{
	FVector2D widgetSize = GetCachedGeometry().GetAbsoluteSize();

	// TODO: offset scaling widgetSize is currently hard-coded, angle dimension widgets should probably
	// have a different pixel offset than the length dimension widgets
	SetPositionInViewport(Position - ((PixelOffset + 2.0f*widgetSize.Y) * OffsetDirection));

	// rotation is value for the transform - math functions supply radians, and the render transform requires degrees
	float rotation = FMath::RadiansToDegrees(FMath::Atan2(OffsetDirection.Y, OffsetDirection.X));

	// the widget should be orthogonal to the direction of the offset and have the text pointing upwards
	rotation += 90.0f;
	rotation = FRotator::ClampAxis(rotation);
	if (rotation > 90.0f && rotation <= 270.0f)
	{
		rotation -= 180.0f;
	}
	Measurement->SetRenderTransformAngle(rotation);

	// set text using the angle argument
	Angle = FRotator::ClampAxis(FMath::RadiansToDegrees(Angle));
	UpdateDegreeText(Angle);
}

void UDimensionWidget::UpdateText(double length, bool bForce)
{
	if ((length != LastMeasurement) || bForce)
	{
		FText newText = UModumateDimensionStatics::CentimetersToDisplayText(length, 1, DisplayUnitType, DisplayOverrideUnit);
		Measurement->SetText(newText);
		LastCommittedText = newText;
		LastMeasurement = length;
		LastDisplayType = EDimensionDisplayType::Linear;
	}
}

void UDimensionWidget::UpdateDegreeText(double angle, bool bForce)
{
	if ((angle != LastMeasurement) || bForce)
	{
		FNumberFormattingOptions options;
		options.MaximumFractionalDigits = 1;
		FText newText = FText::Format(LOCTEXT("degrees", "{0}\u00B0"), FText::AsNumber(angle, &options));
		Measurement->SetText(newText);
		LastCommittedText = newText;
		LastMeasurement = angle;
		LastDisplayType = EDimensionDisplayType::Angular;
	}
}

void UDimensionWidget::UpdateUnits()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (document)
	{
		auto& documentSettings = document->GetCurrentSettings();
		DisplayUnitType = documentSettings.DimensionType;
		DisplayOverrideUnit = documentSettings.DimensionUnit;
	}

	if (LastDisplayType != EDimensionDisplayType::None)
	{
		switch (LastDisplayType)
		{
		case EDimensionDisplayType::Linear:
			UpdateText(LastMeasurement, true);
			break;
		case EDimensionDisplayType::Angular:
			UpdateDegreeText(LastMeasurement, true);
			break;
		default:
			break;
		}
	}
}

void UDimensionWidget::ResetText()
{
	Measurement->SetText(LastCommittedText);
}

void UDimensionWidget::ResetWidget()
{
	LastCommittedText = FText::GetEmpty();
	LastMeasurement = 0.0;
	LastDisplayType = EDimensionDisplayType::None;

	UpdateUnits();
	ResetText();
}

void UDimensionWidget::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto playerHUD = controller ? controller->GetEditModelHUD() : nullptr;
	if (playerHUD)
	{
		TSharedRef<SWidget> slateWidget = TakeWidget();
		slateWidget->SetTag(playerHUD->WorldViewportWidgetTag);
	}
}

#undef LOCTEXT_NAMESPACE
