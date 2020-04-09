// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelToolBase.h"
#include "EditModelSelectTool.h"
#include "ModumateFunctionLibrary.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "Runtime/Engine/Classes/Components/LineBatchComponent.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "ModumateConsoleCommand.h"
#include "ModumateCommands.h"
#include "CompoundMeshActor.h"

/*
* Tool Modes
*/
UEditModelToolBase::UEditModelToolBase(const FObjectInitializer& ObjectInitializer)
	: InUse(false)
	, Active(false)
	, Assembly(false)
	, Controller(nullptr)
	, AxisConstraint(EAxisConstraint::None)
	, CreateObjectMode(EToolCreateObjectMode::Draw)
{
	Controller = Cast<AEditModelPlayerController_CPP>(GetOuter());
}

bool UEditModelToolBase::Activate()
{
	Active = true;
	return true;
}

bool UEditModelToolBase::HandleInputNumber(double n)
{
	return true;
}

bool UEditModelToolBase::Deactivate()
{
	Active = false;
	return true;
}

bool UEditModelToolBase::BeginUse()
{
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(
		Controller->EMPlayerState->SnappedCursor.WorldPosition,
		Controller->EMPlayerState->SnappedCursor.HitNormal,
		Controller->EMPlayerState->SnappedCursor.HitTangent
	);
	InUse = true;
	return true;
}

bool UEditModelToolBase::EnterNextStage()
{
	return false;
}

bool UEditModelToolBase::ScrollToolOption(int32 dir)
{
	return false;
}

bool UEditModelToolBase::FrameUpdate()
{
	return true;
}

bool UEditModelToolBase::EndUse()
{
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	InUse = false;
	return true;
}

bool UEditModelToolBase::AbortUse()
{
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	InUse = false;
	return true;
}
