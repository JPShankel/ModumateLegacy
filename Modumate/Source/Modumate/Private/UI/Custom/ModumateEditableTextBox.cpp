// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateEditableTextBox.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

void UModumateEditableTextBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

bool UModumateEditableTextBox::ApplyCustomStyle()
{
	if (CustomEditableTextBox)
	{
		const FEditableTextBoxStyle* StylePtr = CustomEditableTextBox->GetStyle<FEditableTextBoxStyle>();
		if (StylePtr)
		{
			WidgetStyle = *StylePtr;
			return true;
		}
	}

	return false;
}
