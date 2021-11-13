// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ToolbarBottomWidget.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"


UToolbarBottomWidget::UToolbarBottomWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolbarBottomWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonMainModel && ButtonDrawingDesigner))
	{
		return false;
	}

	ButtonMainModel->ModumateButton->OnReleased.AddDynamic(this, &UToolbarBottomWidget::OnButtonReleaseMainModel);
	ButtonDrawingDesigner->ModumateButton->OnReleased.AddDynamic(this, &UToolbarBottomWidget::OnButtonReleaseDrawingDesigner);

	return true;
}

void UToolbarBottomWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolbarBottomWidget::OnButtonReleaseMainModel()
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller)
	{
		controller->ToggleDrawingDesigner(false);
	}
}

void UToolbarBottomWidget::OnButtonReleaseDrawingDesigner()
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller)
	{
		controller->ToggleDrawingDesigner(true);
	}
}
