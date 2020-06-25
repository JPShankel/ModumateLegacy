// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateTextBlock.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

void UModumateTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

bool UModumateTextBlock::ApplyCustomStyle()
{
	if (CustomTextBlockStyle)
	{
		const FTextBlockStyle* StylePtr = CustomTextBlockStyle->GetStyle<FTextBlockStyle>();
		if (StylePtr)
		{
			SetColorAndOpacity(StylePtr->ColorAndOpacity);
			SetFont(StylePtr->Font);
			SetShadowOffset(StylePtr->ShadowOffset);
			SetShadowColorAndOpacity(StylePtr->ShadowColorAndOpacity);
			return true;
		}
	}

	return false;
}
