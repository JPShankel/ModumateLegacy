#include "EditModelDrawingTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"

#include "LineActor3D_CPP.h"

#include "ModumateCommands.h"

namespace Modumate
{
	FDrawingTool::FDrawingTool(AEditModelPlayerController_CPP *InController)
		: FEditModelToolBase(InController)
	{
		UWorld *world = Controller.IsValid() ? Controller->GetWorld() : nullptr;
		if (ensureAlways(world))
		{
			GameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
		}
	}

	FDrawingTool::~FDrawingTool()
	{
		// empty
	}

	bool FDrawingTool::Activate()
	{
		if (!FEditModelToolBase::Activate())
		{
			return false;
		}
		return true;
	}

	bool FDrawingTool::Deactivate()
	{
		return FEditModelToolBase::Deactivate();
	}

	bool FDrawingTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();

		return EnterNextStage();
	}

	bool FDrawingTool::FrameUpdate()
	{
		FEditModelToolBase::FrameUpdate();

		return true;
	}

	bool FDrawingTool::EnterNextStage()
	{
		FEditModelToolBase::EnterNextStage();

		return true;
	}

	bool FDrawingTool::EndUse()
	{
		return FEditModelToolBase::EndUse();
	}

	bool FDrawingTool::AbortUse()
	{
		EndUse();
		return FEditModelToolBase::AbortUse();
	}

	IModumateEditorTool *MakeDrawingTool(AEditModelPlayerController_CPP *controller)
	{
		return new FDrawingTool(controller);
	}
}