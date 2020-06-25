// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateButton.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

void UModumateButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

bool UModumateButton::ApplyCustomStyle()
{
	if (CustomButtonStyle)
	{
		const FButtonStyle* StylePtr = CustomButtonStyle->GetStyle<FButtonStyle>();
		if (StylePtr)
		{
			// Apply new style, but keep old image size
			FButtonStyle oldStyle = WidgetStyle;
			WidgetStyle = *StylePtr;

			WidgetStyle.Normal.ImageSize = oldStyle.Normal.ImageSize;
			WidgetStyle.Hovered.ImageSize = oldStyle.Hovered.ImageSize;
			WidgetStyle.Pressed.ImageSize = oldStyle.Pressed.ImageSize;
			WidgetStyle.Disabled.ImageSize = oldStyle.Disabled.ImageSize;

			return true;
		}
	}

	return false;
}
