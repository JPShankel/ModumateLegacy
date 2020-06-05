#include "UnrealClasses/DimensionWidget.h"

#include "Components/EditableTextBox.h"
#include "ModumateCore/ModumateUnits.h"

#define LOCTEXT_NAMESPACE "UDimensionWidget"

UDimensionWidget::UDimensionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UDimensionWidget::SetIsEditable(bool bIsEditable)
{
	Measurement->SetIsReadOnly(!bIsEditable);
	!bIsEditable ? Measurement->WidgetStyle.SetFont(ReadOnlyFont) : Measurement->WidgetStyle.SetFont(EditableFont);
}

void UDimensionWidget::UpdateTransform(const FVector2D position, FVector2D edgeDirection, FVector2D offsetDirection, float length)
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

void UDimensionWidget::SanitizeInput(float InLength, FText &OutText)
{
	// TODO: this is assuming the input is in cm, because that's how the data is stored in the volume graph
	InLength /= Modumate::InchesToCentimeters;

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
	if (length != LastLength)
	{
		FText newText;
		SanitizeInput(length, newText);
		Measurement->SetText(newText);
		LastCommittedText = newText;
		LastLength = length;
	}
}

void UDimensionWidget::ResetText()
{
	Measurement->SetText(LastCommittedText);
}

#undef LOCTEXT_NAMESPACE
