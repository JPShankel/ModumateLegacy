// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateCheckBox.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

void UModumateCheckBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

bool UModumateCheckBox::ApplyCustomStyle()
{
	if (CustomCheckbox)
	{
		const FCheckBoxStyle* StylePtr = CustomCheckbox->GetStyle<FCheckBoxStyle>();
		if (StylePtr)
		{
			WidgetStyle = *StylePtr;
			return true;
		}
	}

	return false;
}
