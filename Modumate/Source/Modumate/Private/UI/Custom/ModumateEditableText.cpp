// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateEditableText.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

void UModumateEditableText::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

bool UModumateEditableText::ApplyCustomStyle()
{
	if (CustomEditableText)
	{
		const FEditableTextStyle* StylePtr = CustomEditableText->GetStyle<FEditableTextStyle>();
		if (StylePtr)
		{
			WidgetStyle = *StylePtr;
			return true;
		}
	}

	return false;
}
