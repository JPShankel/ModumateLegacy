// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetScale.h"

#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"

bool UInstPropWidgetScale::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(ScaleText))
	{
		return false;
	}

	ScaleText->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UInstPropWidgetScale::OnScaleTextCommitted);

	return true;
}

void UInstPropWidgetScale::ResetInstProp()
{
	Super::ResetInstProp();

	ValueChangedEvent.Clear();
	CurrentValue = 0.0f;
}

void UInstPropWidgetScale::DisplayValue()
{
	if (bConsistentValue)
	{
		ScaleText->ModumateEditableTextBox->SetHintText(FText::GetEmpty());
		ScaleText->ModumateEditableTextBox->SetText(FText::AsNumber(CurrentValue));
	}
	else
	{
		ScaleText->ModumateEditableTextBox->SetHintText(UInstPropWidgetBase::GetMixedDisplayText());
		ScaleText->ModumateEditableTextBox->SetText(FText::GetEmpty());
	}
}

void UInstPropWidgetScale::RegisterValue(UObject* Source, float ScaleValue)
{
	OnRegisteredValue(Source, CurrentValue == ScaleValue);
	CurrentValue = ScaleValue;
}

void UInstPropWidgetScale::BroadcastValueChanged()
{
	ValueChangedEvent.Broadcast(CurrentValue);
}

void UInstPropWidgetScale::OnScaleTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod)
{
	if ((CommitMethod == ETextCommit::OnEnter) || (CommitMethod == ETextCommit::OnUserMovedFocus))
	{
		float newValue;
		if (LexTryParseString(newValue, *NewText.ToString()) &&  CurrentValue != newValue)
		{
			CurrentValue = newValue;
			PostValueChanged();
		}
	}
}
