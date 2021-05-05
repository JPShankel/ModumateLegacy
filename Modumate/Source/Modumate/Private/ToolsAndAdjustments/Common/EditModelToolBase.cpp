// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "Components/EditableTextBox.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Runtime/Engine/Classes/Components/LineBatchComponent.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "UI/DimensionActor.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"

/*
* Tool Modes
*/
UEditModelToolBase::UEditModelToolBase()
	: Super()
	, InUse(false)
	, Active(false)
	, Controller(nullptr)
	, GameInstance(nullptr)
	, CreateObjectMode(EToolCreateObjectMode::Apply)
	, PendingSegmentID(MOD_ID_NONE)
{
	Controller = Cast<AEditModelPlayerController>(GetOuter());
	if (auto world = GetWorld())
	{
		GameInstance = Cast<UModumateGameInstance>(world->GetGameInstance());
		GameState = world->GetGameState<AEditModelGameState>();
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

	if (HasDimensionActor())
	{
		InitializeDimension();
	}

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
	return PostEndOrAbort();
}

bool UEditModelToolBase::AbortUse()
{
	return PostEndOrAbort();
}

bool UEditModelToolBase::PostEndOrAbort()
{
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	InUse = false;

	if (PendingDimensionActor)
	{
		PendingDimensionActor->DimensionText->Measurement->OnTextCommitted.RemoveDynamic(this, &UEditModelToolBase::OnTextCommitted);
		DimensionManager->SetActiveActorID(MOD_ID_NONE);
		DimensionManager->ReleaseDimensionActor(PendingSegmentID);

		PendingSegmentID = MOD_ID_NONE;
		PendingDimensionActor = nullptr;
		PendingSegment = nullptr;
	}

	return true;
}

void UEditModelToolBase::SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode)
{
	if (CreateObjectMode != InCreateObjectMode)
	{
		CreateObjectMode = InCreateObjectMode;
		OnCreateObjectModeChanged();

		if (Controller)
		{
			Controller->OnToolCreateObjectModeChanged.Broadcast();
		}
	}
}

void UEditModelToolBase::SetAssemblyGUID(const FGuid& InAssemblyGUID)
{
	if (AssemblyGUID != InAssemblyGUID)
	{
		AssemblyGUID = InAssemblyGUID;
		OnAssemblyChanged();

		if (Controller)
		{
			Controller->OnToolAssemblyChanged.Broadcast();
		}
	}
}

void UEditModelToolBase::OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	Controller->OnTextCommitted(Text, CommitMethod);
}

void UEditModelToolBase::InitializeDimension()
{
	PendingDimensionActor = DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
	PendingSegmentID = PendingDimensionActor->ID;
	PendingSegment = PendingDimensionActor->GetLineActor();

	auto dimensionWidget = PendingDimensionActor->DimensionText;
	dimensionWidget->Measurement->SetIsReadOnly(false);
	dimensionWidget->Measurement->OnTextCommitted.AddDynamic(this, &UEditModelToolBase::OnTextCommitted);

	DimensionManager->SetActiveActorID(PendingSegmentID);
}

void UEditModelToolBase::OnCreateObjectModeChanged()
{
}

void UEditModelToolBase::OnAssemblyChanged()
{
}
