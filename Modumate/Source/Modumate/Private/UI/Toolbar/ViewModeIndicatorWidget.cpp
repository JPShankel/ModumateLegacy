// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Toolbar/ViewModeIndicatorWidget.h"
#include "Components/Image.h"
#include "UI/Custom/ModumateTextBlock.h"


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
	return true;
}

void UViewModeIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UViewModeIndicatorWidget::SwitchToViewMode(EEditViewModes NewViewMode)
{
	switch (NewViewMode)
	{
	case EEditViewModes::ObjectEditing:
		ViewModeText->SetText(TextObjectEditing);
		ViewModeImage->SetBrushFromTexture(IconObjectEditing);
		break;
	case EEditViewModes::MetaPlanes:
		ViewModeText->SetText(TextMetaPlanes);
		ViewModeImage->SetBrushFromTexture(IconMetaPlanes);
		break;
	case EEditViewModes::SurfaceGraphs:
		ViewModeText->SetText(TextSurfaceGraphs);
		ViewModeImage->SetBrushFromTexture(IconSurfaceGraphs);
		break;
	default:
		break;
	}
}
