// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCreateSimilarTool.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

#include "DocumentManagement/ModumateCommands.h"

using namespace Modumate;

UCreateSimilarTool::UCreateSimilarTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UCreateSimilarTool::Activate()
{
	MatchTargetObject(false);
	return UEditModelToolBase::Activate();
}

bool UCreateSimilarTool::Deactivate()
{
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
	FModumateObjectInstance *targetObj = nullptr;

	if (bUseMouseHoverObject)
	{
		// Get MOI that's currently under mouse cursor
		targetObj = Controller->EMPlayerState->HoveredObject;
	}
	else
	{
		const auto &selectedObjs = Controller->EMPlayerState->SelectedObjects;
		targetObj = (selectedObjs.Num() > 0) ? *selectedObjs.CreateConstIterator() : nullptr;
	}

	if (targetObj)
	{
		EToolMode targetToolMode = UModumateTypeStatics::ToolModeFromObjectType(targetObj->GetObjectType());
		Controller->EMPlayerState->SetAssemblyForToolMode(targetToolMode, targetObj->GetAssembly().UniqueKey());
		return true;
	}

	return false;
}
