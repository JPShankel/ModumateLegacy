#include "UnrealClasses/DimensionWidget.h"

#include "Components/EditableTextBox.h"
#include "ModumateCore/ModumateUnits.h"
#include "UI/EditModelPlayerHUD.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

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

	return true;
}

void UDimensionWidget::SetIsEditable(bool bIsEditable)
{
	Measurement->SetIsReadOnly(!bIsEditable);
	!bIsEditable ? Measurement->WidgetStyle.SetFont(ReadOnlyFont) : Measurement->WidgetStyle.SetFont(EditableFont);
}

void UDimensionWidget::UpdateLengthTransform(const FVector2D position, FVector2D edgeDirection, FVector2D offsetDirection, float length)
{
	FVector2D widgetSize = GetCachedGeometry().GetAbsoluteSize();

	SetPositionInViewport(position - ((PixelOffset + widgetSize.Y / 2.0f) * offsetDirection));

	float angle = FMath::RadiansToDegrees(FMath::Atan2(edgeDirection.Y, edgeDirection.X));
	angle = FRotator::ClampAxis(angle);

	// flip the text if it is going to appear upside down
	if (angle > 90.0f && angle <= 270.0f)
	{
		angle -= 180.0f;
	}

	Measurement->SetRenderTransformAngle(angle);

	// Update text is called here to make sure that the text matches the current length
	// even if something else changed the length (ex. Undo)
	UpdateText(length);
}

void UDimensionWidget::UpdateDegreeTransform(const FVector2D position, FVector2D offsetDirection, float angle)
{
	FVector2D widgetSize = GetCachedGeometry().GetAbsoluteSize();

	// TODO: offset scaling widgetSize is currently hard-coded, angle dimension widgets should probably
	// have a different pixel offset than the length dimension widgets
	SetPositionInViewport(position - ((PixelOffset + 2.0f*widgetSize.Y) * offsetDirection));

	// rotation is value for the transform - math functions supply radians, and the render transform requires degrees
	float rotation = FMath::RadiansToDegrees(FMath::Atan2(offsetDirection.Y, offsetDirection.X));

	// the widget should be orthogonal to the direction of the offset and have the text pointing upwards
	rotation += 90.0f;
	rotation = FRotator::ClampAxis(rotation);
	if (rotation > 90.0f && rotation <= 270.0f)
	{
		rotation -= 180.0f;
	}
	Measurement->SetRenderTransformAngle(rotation);

	// set text using the angle argument
	angle = FRotator::ClampAxis(FMath::RadiansToDegrees(angle));
	UpdateDegreeText(angle);
}

void UDimensionWidget::SanitizeInput(float InLength, FText &OutText)
{
	// TODO: this is assuming the input is in cm, because that's how the data is stored in the volume graph
	InLength /= UModumateDimensionStatics::InchesToCentimeters;

	// TODO: potentially this is a project setting
	int32 tolerance = 64;

	int32 feet = InLength / 12;

	InLength -= (feet * 12);

	int32 inches = InLength;

	InLength -= inches;

	InLength *= tolerance;

	// rounding here allows for rounding based on the tolerance
	// (ex. rounding to the nearest 1/64")
	int32 numerator = FMath::RoundHalfToZero(InLength);
	int32 denominator = tolerance;
	while (numerator % 2 == 0 && numerator != 0)
	{
		numerator /= 2;
		denominator /= 2;
	}
	// carry
	if (denominator == 1)
	{
		inches++;
		numerator = 0;
	}
	if (inches == 12)
	{
		feet++;
		inches -= 12;
	}

	FText feetText = feet != 0 ? FText::Format(LOCTEXT("feet", "{0}'-"), feet) : FText();
	FText inchesText;

	if (numerator != 0)
	{
		inchesText = FText::Format(LOCTEXT("inches_frac", "{0} {1}/{2}\""), inches, numerator, denominator);
	}
	else
	{
		inchesText = FText::Format(LOCTEXT("inches", "{0}\""), inches);
	}

	// if there are both feet and inches, separate with a dash, otherwise use the one that exists
	OutText = FText::Format(LOCTEXT("feet_and_inches", "{0}{1}"), feetText, inchesText);
}

void UDimensionWidget::UpdateText(float length)
{
	if (length != LastMeasurement)
	{
		FText newText;
		SanitizeInput(length, newText);
		Measurement->SetText(newText);
		LastCommittedText = newText;
		LastMeasurement = length;
	}
}

void UDimensionWidget::UpdateDegreeText(float angle)
{
	if (angle != LastMeasurement)
	{
		FNumberFormattingOptions options;
		options.MaximumFractionalDigits = 1;
		FText newText = FText::Format(LOCTEXT("degrees", "{0}°"), FText::AsNumber(angle, &options));
		Measurement->SetText(newText);
		LastCommittedText = newText;
		LastMeasurement = angle;
	}
}

void UDimensionWidget::ResetText()
{
	Measurement->SetText(LastCommittedText);
}

void UDimensionWidget::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	auto controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	auto playerHUD = controller ? controller->GetEditModelHUD() : nullptr;
	if (playerHUD)
	{
		TSharedRef<SWidget> slateWidget = TakeWidget();
		slateWidget->SetTag(playerHUD->WorldViewportWidgetTag);
	}
}

#undef LOCTEXT_NAMESPACE
