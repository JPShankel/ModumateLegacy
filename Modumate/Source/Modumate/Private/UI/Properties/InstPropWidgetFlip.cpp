// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetFlip.h"

#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"


bool UInstPropWidgetFlip::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(ButtonFlipX && ButtonFlipY && ButtonFlipZ))
	{
		return false;
	}

	ButtonFlipX->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetFlip::OnClickedFlipX);
	ButtonFlipY->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetFlip::OnClickedFlipY);
	ButtonFlipZ->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetFlip::OnClickedFlipZ);

	return true;
}

void UInstPropWidgetFlip::ResetInstProp()
{
	Super::ResetInstProp();

	ValueChangedEvent.Clear();
	EnabledAxes = EAxisList::None;
}

void UInstPropWidgetFlip::DisplayValue()
{
	ButtonFlipX->SetVisibility((EnabledAxes & EAxisList::X) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonFlipY->SetVisibility((EnabledAxes & EAxisList::Y) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonFlipZ->SetVisibility((EnabledAxes & EAxisList::Z) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UInstPropWidgetFlip::RegisterValue(UObject* Source, EAxisList::Type RequestedAxes)
{
	OnRegisteredValue(Source, EnabledAxes == RequestedAxes);
	EnabledAxes = static_cast<EAxisList::Type>(static_cast<int32>(EnabledAxes) | static_cast<int32>(RequestedAxes));
}

void UInstPropWidgetFlip::BroadcastValueChanged()
{
	ValueChangedEvent.Broadcast(static_cast<int32>(LastClickedAxis));
	LastClickedAxis = EAxis::None;
}

void UInstPropWidgetFlip::OnClickedFlipX()
{
	LastClickedAxis = EAxis::X;
	PostValueChanged();
}

void UInstPropWidgetFlip::OnClickedFlipY()
{
	LastClickedAxis = EAxis::Y;
	PostValueChanged();
}

void UInstPropWidgetFlip::OnClickedFlipZ()
{
	LastClickedAxis = EAxis::Z;
	PostValueChanged();
}
