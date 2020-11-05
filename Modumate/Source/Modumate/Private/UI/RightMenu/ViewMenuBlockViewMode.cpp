// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuBlockViewMode.h"

#include "UI/Custom/ModumateButtonUserWidget.h"

bool UViewMenuBlockViewMode::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(Button_MetaGraphMode && Button_SeparatorsMode && Button_SurfaceGraphsMode && Button_AllObjectsMode && Button_PhysicalMode))
	{
		return false;
	}

	ViewModesToButtons.Add(EEditViewModes::MetaGraph, Button_MetaGraphMode);
	ViewModesToButtons.Add(EEditViewModes::Separators, Button_SeparatorsMode);
	ViewModesToButtons.Add(EEditViewModes::SurfaceGraphs, Button_SurfaceGraphsMode);
	ViewModesToButtons.Add(EEditViewModes::AllObjects, Button_AllObjectsMode);
	ViewModesToButtons.Add(EEditViewModes::Physical, Button_PhysicalMode);

	return true;
}

void UViewMenuBlockViewMode::UpdateEnabledViewModes(const TArray<EEditViewModes>& EnabledViewModes)
{
	for (auto& kvp : ViewModesToButtons)
	{
		kvp.Value->SetIsEnabled(EnabledViewModes.Contains(kvp.Key));
	}
}

void UViewMenuBlockViewMode::SetActiveViewMode(EEditViewModes ViewMode)
{
	for (auto& kvp : ViewModesToButtons)
	{
		if (kvp.Key == ViewMode)
		{
			kvp.Value->SwitchToActiveStyle();
		}
		else
		{
			kvp.Value->SwitchToNormalStyle();
		}
	}
}
