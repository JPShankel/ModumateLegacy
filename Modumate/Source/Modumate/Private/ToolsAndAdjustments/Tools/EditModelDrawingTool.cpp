#include "ToolsAndAdjustments/Tools/EditModelDrawingTool.h"

#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"

#include "UnrealClasses/LineActor.h"

#include "DocumentManagement/ModumateCommands.h"

UDrawingTool::UDrawingTool()
	: Super()
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	GameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
}

bool UDrawingTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}
	return true;
}

bool UDrawingTool::Deactivate()
{
	return UEditModelToolBase::Deactivate();
}

bool UDrawingTool::BeginUse()
{
	UEditModelToolBase::BeginUse();

	return EnterNextStage();
}

bool UDrawingTool::FrameUpdate()
{
	UEditModelToolBase::FrameUpdate();

	return true;
}

bool UDrawingTool::EnterNextStage()
{
	UEditModelToolBase::EnterNextStage();

	return true;
}

bool UDrawingTool::EndUse()
{
	return UEditModelToolBase::EndUse();
}

bool UDrawingTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}
