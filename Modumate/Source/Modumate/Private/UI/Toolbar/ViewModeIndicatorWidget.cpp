// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ViewModeIndicatorWidget.h"
#include "Components/Image.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/TooltipManager.h"


UViewModeIndicatorWidget::UViewModeIndicatorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UViewModeIndicatorWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (IndicatorButton)
	{
		IndicatorButton->OnReleased.AddDynamic(this, &UViewModeIndicatorWidget::OnIndicatorButtonPress);
	}
	ToolTipWidgetDelegate.BindDynamic(this, &UViewModeIndicatorWidget::OnTooltipWidget);

	return true;
}

void UViewModeIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UViewModeIndicatorWidget::OnIndicatorButtonPress()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller)
	{
		controller->InputHandlerComponent->TryCommand(InputCommand);
	}
}

void UViewModeIndicatorWidget::SwitchToViewMode(EEditViewModes NewViewMode)
{
	switch (NewViewMode)
	{
	case EEditViewModes::MetaGraph:
		ViewModeText->SetText(TextMetaPlanes);
		ViewModeImage->SetBrushFromTexture(IconMetaPlanes);
		break;
	case EEditViewModes::Separators:
		ViewModeText->SetText(TextSeparators);
		ViewModeImage->SetBrushFromTexture(IconSeparators);
		break;
	case EEditViewModes::SurfaceGraphs:
		ViewModeText->SetText(TextSurfaceGraphs);
		ViewModeImage->SetBrushFromTexture(IconSurfaceGraphs);
		break;
	case EEditViewModes::AllObjects:
		ViewModeText->SetText(TextAllObjects);
		ViewModeImage->SetBrushFromTexture(IconAllObjects);
		break;
	case EEditViewModes::Physical:
		ViewModeText->SetText(TextPhysical);
		ViewModeImage->SetBrushFromTexture(IconPhysical);
		break;
	default:
		break;
	}
}

UWidget* UViewModeIndicatorWidget::OnTooltipWidget()
{
	return UTooltipManager::GenerateTooltipWithInputWidget(InputCommand, this);
}
