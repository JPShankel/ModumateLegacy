#include "UnrealClasses/DimensionWidget.h"

#include "Components/EditableTextBox.h"
#include "ModumateCore/ModumateUnits.h"
#include "UI/EditModelPlayerHUD.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"

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

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto gameInstance = controller ? controller->GetGameInstance<UModumateGameInstance>() : nullptr;
	if (gameInstance)
	{
		DisplayUnitType = gameInstance->UserSettings.PreferredDimensionType;
		DisplayOverrideUnit = gameInstance->UserSettings.PreferredDimensionUnit;
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

void UDimensionWidget::UpdateText(float length)
{
	if (length != LastMeasurement)
	{
		FText newText = UModumateDimensionStatics::CentimetersToDisplayText(length, DisplayUnitType, DisplayOverrideUnit);
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

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto playerHUD = controller ? controller->GetEditModelHUD() : nullptr;
	if (playerHUD)
	{
		TSharedRef<SWidget> slateWidget = TakeWidget();
		slateWidget->SetTag(playerHUD->WorldViewportWidgetTag);
	}
}

#undef LOCTEXT_NAMESPACE
