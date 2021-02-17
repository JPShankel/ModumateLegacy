// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetRotation.h"

#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"


bool UInstPropWidgetRotation::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(RotationText && RotationTextSuffix))
	{
		return false;
	}

	RotationText->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UInstPropWidgetRotation::OnRotationTextCommitted);

	return true;
}

void UInstPropWidgetRotation::ResetInstProp()
{
	Super::ResetInstProp();

	ValueChangedEvent.Clear();
	CurrentValue = 0.0f;
}

void UInstPropWidgetRotation::DisplayValue()
{
	if (bConsistentValue)
	{
		RotationText->ModumateEditableTextBox->SetHintText(FText::GetEmpty());
		RotationText->ModumateEditableTextBox->SetText(FText::AsNumber(CurrentValue));
		RotationTextSuffix->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		RotationText->ModumateEditableTextBox->SetHintText(UInstPropWidgetBase::GetMixedDisplayText());
		RotationText->ModumateEditableTextBox->SetText(FText::GetEmpty());
		RotationTextSuffix->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInstPropWidgetRotation::RegisterValue(UObject* Source, float RotationValue)
{
	OnRegisteredValue(Source, CurrentValue == RotationValue);
	CurrentValue = RotationValue;
}

void UInstPropWidgetRotation::BroadcastValueChanged()
{
	ValueChangedEvent.Broadcast(CurrentValue);
}

void UInstPropWidgetRotation::OnRotationTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod)
{
	if ((CommitMethod == ETextCommit::OnEnter) || (CommitMethod == ETextCommit::OnUserMovedFocus))
	{
		float newValue;
		if (LexTryParseString(newValue, *NewText.ToString()) && (CurrentValue != newValue))
		{
			CurrentValue = newValue;
			PostValueChanged();
		}
	}
}
