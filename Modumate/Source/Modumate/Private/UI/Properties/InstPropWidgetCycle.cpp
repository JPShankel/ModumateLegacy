// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetCycle.h"

#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"


bool UInstPropWidgetCycle::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (CycleBasisButton == nullptr)
		return true;
	CycleBasisButton->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetCycle::OnClickedCycleBtn);
	return true;
}

void UInstPropWidgetCycle::ResetInstProp()
{
	Super::ResetInstProp();

	ValueChangedEvent.Clear();
	LastBasisValue = 0;
}

void UInstPropWidgetCycle::DisplayValue()
{
	CycleBasisButton->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UInstPropWidgetCycle::RegisterValue(UObject* Source)
{
	LastBasisValue = 0;
	OnRegisteredValue(Source, true);
}

void UInstPropWidgetCycle::BroadcastValueChanged()
{
	ValueChangedEvent.Broadcast(static_cast<int32>(LastBasisValue));
	//LastClickedAxis = EAxis::None;
}

void UInstPropWidgetCycle::OnClickedCycleBtn()
{
	LastBasisValue++;
	PostValueChanged();
}