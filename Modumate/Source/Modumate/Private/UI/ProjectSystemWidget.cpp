// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ProjectSystemWidget.h"

#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"


bool UProjectSystemWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(ButtonNewProject && ButtonSaveProject && ButtonSaveProjectAs && ButtonLoadProject && ButtonMainMenu))
	{
		return false;
	}

	ButtonNewProject->ModumateButton->OnReleased.AddDynamic(this, &UProjectSystemWidget::OnPressedButton);
	ButtonSaveProject->ModumateButton->OnReleased.AddDynamic(this, &UProjectSystemWidget::OnPressedButton);
	ButtonSaveProjectAs->ModumateButton->OnReleased.AddDynamic(this, &UProjectSystemWidget::OnPressedButton);
	ButtonLoadProject->ModumateButton->OnReleased.AddDynamic(this, &UProjectSystemWidget::OnPressedButton);
	ButtonMainMenu->ModumateButton->OnReleased.AddDynamic(this, &UProjectSystemWidget::OnPressedButton);

	return true;
}

void UProjectSystemWidget::OnPressedButton()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
