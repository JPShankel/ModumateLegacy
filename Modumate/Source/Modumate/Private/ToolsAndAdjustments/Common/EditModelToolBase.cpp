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
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameMode.h"
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
		GameMode = GameInstance ? GameInstance->GetEditModelGameMode() : nullptr;
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

	if (EdgeDimensionID)
	{
		DimensionManager->ReleaseDimensionActor(EdgeDimensionID);
		EdgeDimensionID = MOD_ID_NONE;
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

bool UEditModelToolBase::ApplyStateData(const FMOIStateData& InStateData)
{
	NewMOIStateData = InStateData;
	return true;
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

bool UEditModelToolBase::IsObjectInActiveGroup(const AModumateObjectInstance* MOI) const
{
	while (MOI && UModumateTypeStatics::Graph3DObjectTypeFromObjectType(MOI->GetObjectType()) == EGraph3DObjectType::None)
	{
		MOI = MOI->GetParentObject();
	}
	if (MOI == nullptr)
	{
		return true;  // Non-graph moi is always accessible.
	}

	const auto* doc = Controller->GetDocument();
	const FGraph3D* graph = doc->FindVolumeGraph(MOI->ID);
	return graph->GraphID == doc->GetActiveVolumeGraphID();
}

// Display length along any snapped line from nearest end.
void UEditModelToolBase::UpdateEdgeDimension(const FSnappedCursor& snappedCursor)
{
	ALineActor* snappedEdgeActor = Cast<ALineActor>(snappedCursor.Actor);
	if (snappedEdgeActor && snappedCursor.SnapType != ESnapType::CT_MIDSNAP && snappedCursor.CP1 != -1)
	{
		ADimensionActor* edgeDimensionActor = nullptr;
		if (EdgeDimensionID == MOD_ID_NONE)
		{
			edgeDimensionActor = DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
			EdgeDimensionID = edgeDimensionActor->ID;
			edgeDimensionActor->DimensionText->Measurement->SetIsReadOnly(true);
			edgeDimensionActor->GetLineActor()->SetVisibilityInApp(false);
		}
		else
		{
			edgeDimensionActor = DimensionManager->GetDimensionActor(EdgeDimensionID);
		}

		if (edgeDimensionActor && snappedEdgeActor)
		{
			const FVector& hitLocation = snappedCursor.HasProjectedPosition ? snappedCursor.ProjectedPosition : snappedCursor.WorldPosition;
			const FVector& closestPoint =
				FVector::DistSquared(hitLocation, snappedEdgeActor->Point1) < FVector::DistSquared(hitLocation, snappedEdgeActor->Point2) ?
				snappedEdgeActor->Point1 : snappedEdgeActor->Point2;

			
			auto* edgeDimensionSegment = edgeDimensionActor->GetLineActor();
			edgeDimensionSegment->Point1 = closestPoint;
			edgeDimensionSegment->Point2 = hitLocation;
			
			ESlateVisibility visibility = ESlateVisibility::Visible;
			if (PendingDimensionActor)
			{   // Don't overlay the pending segment dimension. Just test equality of FVectors
				// since values are copied from MOI or snapped cursor.
				auto* pendingSegmentActor = PendingDimensionActor->GetLineActor();
				if ((pendingSegmentActor->Point1 == edgeDimensionSegment->Point1 && pendingSegmentActor->Point2 == edgeDimensionSegment->Point2)
					|| (pendingSegmentActor->Point1 == edgeDimensionSegment->Point2 && pendingSegmentActor->Point2 == edgeDimensionSegment->Point1))
				{
					visibility = ESlateVisibility::Hidden;
				}
			}

			DimensionManager->GetDimensionActor(EdgeDimensionID)->DimensionText->SetVisibility(visibility);
		}
	}
	else if (EdgeDimensionID != MOD_ID_NONE)
	{
		DimensionManager->GetDimensionActor(EdgeDimensionID)->DimensionText->SetVisibility(ESlateVisibility::Hidden);
	}
}
