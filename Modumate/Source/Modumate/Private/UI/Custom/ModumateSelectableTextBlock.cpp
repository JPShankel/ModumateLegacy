// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateSelectableTextBlock.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"



void UModumateSelectableTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

bool UModumateSelectableTextBlock::ApplyCustomStyle()
{
	if (CustomEditableText)
	{
		const FEditableTextBoxStyle* StylePtr = CustomEditableText->GetStyle<FEditableTextBoxStyle>();
		if (StylePtr)
		{
			WidgetStyle = *StylePtr;
			return true;
		}
	}

	return false;
}
