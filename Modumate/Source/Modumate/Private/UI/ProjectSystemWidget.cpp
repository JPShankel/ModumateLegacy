// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ProjectSystemWidget.h"

#include "Components/VerticalBox.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"


bool UProjectSystemWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(ButtonBox))
	{
		return false;
	}

	int32 numButtons = ButtonBox->GetChildrenCount();
	for (int32 buttonIdx = 0; buttonIdx < numButtons; ++buttonIdx)
	{
		auto* button = Cast<UModumateButtonUserWidget>(ButtonBox->GetChildAt(buttonIdx));
		if (button)
		{
			button->ModumateButton->OnReleased.AddDynamic(this, &UProjectSystemWidget::OnPressedButton);
		}
	}

	return true;
}

void UProjectSystemWidget::OnPressedButton()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
