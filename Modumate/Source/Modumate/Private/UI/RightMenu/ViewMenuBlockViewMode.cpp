// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuBlockViewMode.h"

#include "UI/Custom/ModumateButtonUserWidget.h"

bool UViewMenuBlockViewMode::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(Button_ObjectMode && Button_SurfaceGraphMode && Button_MetaplaneMode))
	{
		return false;
	}

	EditModesToButtons.Add(EEditViewModes::ObjectEditing, Button_ObjectMode);
	EditModesToButtons.Add(EEditViewModes::MetaPlanes, Button_MetaplaneMode);
	EditModesToButtons.Add(EEditViewModes::SurfaceGraphs, Button_SurfaceGraphMode);

	return true;
}

void UViewMenuBlockViewMode::UpdateEnabledEditModes(const TArray<EEditViewModes>& EnabledEditModes)
{
	for (auto& kvp : EditModesToButtons)
	{
		kvp.Value->SetIsEnabled(EnabledEditModes.Contains(kvp.Key));
	}
}

void UViewMenuBlockViewMode::SetActiveEditMode(EEditViewModes EditMode)
{
	for (auto& kvp : EditModesToButtons)
	{
		if (kvp.Key == EditMode)
		{
			kvp.Value->SwitchToActiveStyle();
		}
		else
		{
			kvp.Value->SwitchToNormalStyle();
		}
	}
}
