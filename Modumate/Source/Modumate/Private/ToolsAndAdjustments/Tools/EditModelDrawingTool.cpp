#include "EditModelDrawingTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"

#include "LineActor3D_CPP.h"

#include "ModumateCommands.h"

UDrawingTool::UDrawingTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	GameMode = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
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
