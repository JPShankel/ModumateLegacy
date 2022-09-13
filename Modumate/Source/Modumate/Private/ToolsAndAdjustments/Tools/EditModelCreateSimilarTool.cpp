// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCreateSimilarTool.h"

#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

#include "DocumentManagement/ModumateCommands.h"



UCreateSimilarTool::UCreateSimilarTool()
	: Super()
{

}

bool UCreateSimilarTool::Activate()
{
	MatchTargetObject(false);
	Controller->EMPlayerState->SetShowHoverEffects(true);
	return UEditModelToolBase::Activate();
}

bool UCreateSimilarTool::Deactivate()
{
	Controller->EMPlayerState->SetShowHoverEffects(false);
	return UEditModelToolBase::Deactivate();
}

bool UCreateSimilarTool::BeginUse()
{
	return UEditModelToolBase::BeginUse();
}

bool UCreateSimilarTool::HandleMouseUp()
{
	MatchTargetObject(true);
	return true;
}

bool UCreateSimilarTool::EnterNextStage()
{
	return true;
}

bool UCreateSimilarTool::FrameUpdate()
{
	Super::FrameUpdate();

	return true;
}

bool UCreateSimilarTool::EndUse()
{
	return UEditModelToolBase::EndUse();
}

bool UCreateSimilarTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}


bool UCreateSimilarTool::MatchTargetObject(bool bUseMouseHoverObject)
{
	AModumateObjectInstance *targetObj = nullptr;

	if (bUseMouseHoverObject)
	{
		// Get MOI that's currently under mouse cursor
		targetObj = Controller->EMPlayerState->HoveredObject;
	}
	else
	{
		const auto& selectedObjs = Controller->EMPlayerState->SelectedObjects;
		const auto& selectedGroups = Controller->EMPlayerState->SelectedGroupObjects;
		targetObj = (selectedObjs.Num() == 1) ? *selectedObjs.CreateConstIterator() :
			(selectedGroups.Num() == 1) ? *selectedGroups.CreateConstIterator() : nullptr;
	}

	if (targetObj)
	{
		EToolMode targetToolMode = UModumateTypeStatics::ToolModeFromObjectType(targetObj->GetObjectType());
		Controller->EMPlayerState->SetAssemblyForToolMode(targetToolMode, targetObj->GetAssembly().UniqueKey());
		Controller->SetToolMode(targetToolMode);
		Controller->SetToolCreateObjectMode(EToolCreateObjectMode::Apply);
		Controller->CurrentTool->ApplyStateData(targetObj->GetStateData());

		return true;
	}

	return false;
}
