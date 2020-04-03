// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelToolBase.h"
#include "EditModelSelectTool.h"
#include "ModumateFunctionLibrary.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "Runtime/Engine/Classes/Components/LineBatchComponent.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "ModumateConsoleCommand.h"
#include "ModumateCommands.h"
#include "CompoundMeshActor.h"

/*
* Tool Modes
*/
namespace Modumate
{
	FEditModelToolBase::FEditModelToolBase(AEditModelPlayerController_CPP *pc)
		: InUse(false)
		, Active(false)
		, Assembly(false)
		, Controller(pc)
		, AxisConstraint(EAxisConstraint::None)
		, CreateObjectMode(EToolCreateObjectMode::Draw)
	{ }

	bool FEditModelToolBase::Activate()
	{
		Active = true;
		return true;
	}

	bool FEditModelToolBase::HandleInputNumber(double n)
	{
		return true;
	}

	bool FEditModelToolBase::Deactivate()
	{
		Active = false;
		return true;
	}

	bool FEditModelToolBase::BeginUse()
	{		
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(
			Controller->EMPlayerState->SnappedCursor.WorldPosition,
			Controller->EMPlayerState->SnappedCursor.HitNormal,
			Controller->EMPlayerState->SnappedCursor.HitTangent
		);
		InUse = true;
		return true;
	}

	bool FEditModelToolBase::EnterNextStage()
	{
		return false;
	}

	bool FEditModelToolBase::ScrollToolOption(int32 dir)
	{
		return false;
	}

	bool FEditModelToolBase::FrameUpdate()
	{
		return true;
	}

	bool FEditModelToolBase::EndUse()
	{
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		InUse = false;
		return true;
	}

	bool FEditModelToolBase::AbortUse()
	{
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		InUse = false;
		return true;
	}
}
