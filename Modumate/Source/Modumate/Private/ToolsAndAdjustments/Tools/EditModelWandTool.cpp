// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelWandTool.h"
#include "ModumateFunctionLibrary.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "ModumateSnappingView.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"

namespace Modumate
{
	FWandTool::FWandTool(AEditModelPlayerController_CPP *controller) :
		FEditModelToolBase(controller)
	{}

	FWandTool::~FWandTool()
	{}

	bool FWandTool::Activate()
	{
		FEditModelToolBase::Activate();

		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;
		return true;
	}

	bool FWandTool::Deactivate()
	{
		return FEditModelToolBase::Deactivate();
	}


	bool FWandTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();

		return true;
	}

	bool FWandTool::HandleMouseUp()
	{

		AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		Modumate::FModumateDocument *doc = &gameState->Document;

		FModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;
		if (newTarget)
		{
			UModumateFunctionLibrary::DocAddHideMoiActors(TArray<AActor*> {newTarget->GetActor()});
		}
		EndUse();

		return true;
	}

	bool FWandTool::FrameUpdate()
	{
		FEditModelToolBase::FrameUpdate();
		return true;
	}

	bool FWandTool::EndUse()
	{
		FEditModelToolBase::EndUse();
		return true;
	}


	IModumateEditorTool *MakeWandTool(AEditModelPlayerController_CPP *controller)
	{
		return new FWandTool(controller);
	}

}