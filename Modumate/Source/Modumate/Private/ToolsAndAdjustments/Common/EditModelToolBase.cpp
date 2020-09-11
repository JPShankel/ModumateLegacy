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
		GameInstance = Cast<UModumateGameInstance>(world->GetGameInstance());
		GameState = world->GetGameState<AEditModelGameState_CPP>();
		DimensionManager = GameInstance ? GameInstance->DimensionManager : nullptr;
	}
}

bool UEditModelToolBase::Activate()
{
	if (!ensure(Controller && GameInstance && GameState && DimensionManager))
	{
		return false;
	}

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
	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	cursor.SetAffordanceFrame(cursor.WorldPosition, cursor.HitNormal, cursor.HitTangent, true, true);

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
