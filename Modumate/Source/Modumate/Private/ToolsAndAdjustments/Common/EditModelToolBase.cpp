// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Runtime/Engine/Classes/Components/LineBatchComponent.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"

/*
* Tool Modes
*/
UEditModelToolBase::UEditModelToolBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InUse(false)
	, Active(false)
	, Controller(nullptr)
	, GameInstance(nullptr)
	, AxisConstraint(EAxisConstraint::None)
	, CreateObjectMode(EToolCreateObjectMode::Draw)
{
	Controller = Cast<AEditModelPlayerController_CPP>(GetOuter());
	if (auto world = GetWorld())
	{
		GameInstance = Cast<UModumateGameInstance>(GetWorld()->GetGameInstance());
	}
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
