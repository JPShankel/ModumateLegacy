// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelCreateSimilarTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"

#include "ModumateCommands.h"

namespace Modumate
{
	FCreateSimilarTool::FCreateSimilarTool(AEditModelPlayerController_CPP *InController)
		: FEditModelToolBase(InController)
	{

	}

	FCreateSimilarTool::~FCreateSimilarTool() {}

	bool FCreateSimilarTool::Activate()
	{
		GetCreateSimilarActor(false);
		return FEditModelToolBase::Activate();
	}

	bool FCreateSimilarTool::Deactivate()
	{
		return FEditModelToolBase::Deactivate();
	}

	bool FCreateSimilarTool::BeginUse()
	{
		return FEditModelToolBase::BeginUse();
	}

	bool FCreateSimilarTool::HandleMouseUp()
	{
		GetCreateSimilarActor(true);
		return true;
	}

	bool FCreateSimilarTool::EnterNextStage()
	{
		return true;
	}

	bool FCreateSimilarTool::FrameUpdate()
	{
		FEditModelToolBase::FrameUpdate();

		return true;
	}

	bool FCreateSimilarTool::EndUse()
	{
		return FEditModelToolBase::EndUse();
	}

	bool FCreateSimilarTool::AbortUse()
	{
		EndUse();
		return FEditModelToolBase::AbortUse();
	}


	bool FCreateSimilarTool::GetCreateSimilarActor(bool bUseMouseHoverObject)
	{
		if (bUseMouseHoverObject)
		{
			// Get actor that's currently under mouse cursor

			FModumateObjectInstance *moi = Controller->EMPlayerState->HoveredObject;
			if ((moi != nullptr) && (moi->GetActor() != nullptr))
			{
				Controller->EMPlayerState->RefreshCreateSimilarActor(moi->GetActor());
				return true;
			}
		}
		else
		{
			// Get currently selected actor. If there's more than one, use the first actor
			TArray<AActor*> selectedActors;
			Controller->EMPlayerState->GetSelectorModumateObjects(selectedActors);

			if ((selectedActors.Num() > 0) && (selectedActors[0] != nullptr))
			{
				Controller->EMPlayerState->RefreshCreateSimilarActor(selectedActors[0]);
				return true;
			}
		}

		return false;
	}

	IModumateEditorTool *MakeCreateSimilarTool(AEditModelPlayerController_CPP *controller)
	{
		return new FCreateSimilarTool(controller);
	}
}