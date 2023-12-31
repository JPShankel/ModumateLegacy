// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetLinearDimension.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"


bool UInstPropWidgetLinearDimension::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(DimensionText))
	{
		return false;
	}

	DimensionText->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UInstPropWidgetLinearDimension::OnDimensionTextCommitted);

	return true;
}

void UInstPropWidgetLinearDimension::ResetInstProp()
{
	Super::ResetInstProp();

	ValueChangedEvent.Clear();
	CurrentValue = 0.0f;
}

void UInstPropWidgetLinearDimension::DisplayValue()
{
	if (bConsistentValue)
	{
		auto controller = GetOwningPlayer<AEditModelPlayerController>();
		auto document = controller ? controller->GetDocument() : nullptr;
		if (!ensure(document))
		{
			return;
		}

		auto& documentSettings = document->GetCurrentSettings();
		FText valueText = UModumateDimensionStatics::CentimetersToDisplayText(CurrentValue, 1,
			documentSettings.DimensionType, documentSettings.DimensionUnit,
			DisplayFractionDenomPow,
			UModumateDimensionStatics::DefaultRoundingTolerance, UModumateDimensionStatics::DefaultRoundingDigits,
			NumDisplayDecimalDigits);

		DimensionText->ModumateEditableTextBox->SetHintText(FText::GetEmpty());
		DimensionText->ModumateEditableTextBox->SetText(valueText);
	}
	else
	{
		DimensionText->ModumateEditableTextBox->SetHintText(UInstPropWidgetBase::GetMixedDisplayText());
		DimensionText->ModumateEditableTextBox->SetText(FText::GetEmpty());
	}
}

void UInstPropWidgetLinearDimension::RegisterValue(UObject* Source, float DimensionValue)
{
	OnRegisteredValue(Source, CurrentValue == DimensionValue);
	CurrentValue = DimensionValue;
}

void UInstPropWidgetLinearDimension::BroadcastValueChanged()
{
	ValueChangedEvent.Broadcast(CurrentValue);
}

void UInstPropWidgetLinearDimension::OnDimensionTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod)
{
	if ((CommitMethod == ETextCommit::OnEnter) || (CommitMethod == ETextCommit::OnUserMovedFocus))
	{
		auto dimension = UModumateDimensionStatics::StringToFormattedDimension(NewText.ToString());
		if ((dimension.Format != EDimensionFormat::Error) && (dimension.Centimeters != CurrentValue))
		{
			CurrentValue = dimension.Centimeters;
			PostValueChanged();
		}
	}
}
