// Copyright 2021 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateSlider.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

void UModumateSlider::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

bool UModumateSlider::ApplyCustomStyle()
{
	if (CustomSliderStyleAsset)
	{
		const FSliderStyle* StylePtr = CustomSliderStyleAsset->GetStyle<FSliderStyle>();
		if (StylePtr)
		{
			WidgetStyle = *StylePtr;
			return true;
		}
	}

	return false;
}
