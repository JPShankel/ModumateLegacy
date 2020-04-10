// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelCreateSimilarTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"

#include "ModumateCommands.h"

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
		targetObj = (selectedObjs.Num() > 0) ? selectedObjs[0] : nullptr;
	}

	if (targetObj)
	{
		FShoppingItem targetShoppingItem = targetObj->GetAssembly().AsShoppingItem();
		EToolMode targetToolMode = UModumateTypeStatics::ToolModeFromObjectType(targetObj->GetObjectType());
		Controller->EMPlayerState->SetAssemblyForToolMode(targetToolMode, targetShoppingItem);
		return true;
	}

	return false;
}
