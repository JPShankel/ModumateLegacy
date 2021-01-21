// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelLineTool.h"

#include "Components/EditableTextBox.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/LineActor.h"

ULineTool::ULineTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	if (world)
	{
		GameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
		GameState = world->GetGameState<AEditModelGameState_CPP>();
	}
}

void ULineTool::Initialize()
{

}

bool ULineTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	return UEditModelToolBase::Activate();
}

bool ULineTool::HandleInputNumber(double n)
{
	Super::HandleInputNumber(n);

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	auto pendingSegment = DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();

	if ((State == NewSegmentPending) && pendingSegment != nullptr)
	{
		FVector dir = (pendingSegment->Point2 - pendingSegment->Point1).GetSafeNormal() * n;

		return MakeObject(pendingSegment->Point1 + dir);
	}

	return true;
}

bool ULineTool::Deactivate()
{
	State = Neutral;
	Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	return UEditModelToolBase::Deactivate();
}

bool ULineTool::BeginUse()
{
	Super::BeginUse();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;

	const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	if (dimensionActor != nullptr)
	{
		auto pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = hitLoc;
		pendingSegment->Point2 = hitLoc;
		pendingSegment->SetVisibilityInApp(false);
	}

	State = NewSegmentPending;
	GameState->Document->StartPreviewing();

	return true;
}

bool ULineTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	if (State == NewSegmentPending)
	{
		return MakeObject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
	}

	return false;
}

bool ULineTool::FrameUpdate()
{
	Super::FrameUpdate();

	CurDeltas.Reset();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return true;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (State != NewSegmentPending || !ensure(GameState && GameState->Document))
	{
		return true;
	}

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	ALineActor* pendingSegment = nullptr;
	if (dimensionActor != nullptr)
	{
		pendingSegment = dimensionActor->GetLineActor();
	}
	pendingSegment->Point2 = hitLoc;

	GameState->Document->StartPreviewing();

	GetEdgeDeltas(pendingSegment->Point1, pendingSegment->Point2, true);

	GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());

	return true;
}

bool ULineTool::EndUse()
{
	State = Neutral;

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return Super::EndUse();
}

bool ULineTool::AbortUse()
{
	EndUse();
	return Super::AbortUse();
}

bool ULineTool::PostEndOrAbort()
{
	if (GameState && GameState->Document)
	{
		GameState->Document->ClearPreviewDeltas(GetWorld(), false);
	}

	return Super::PostEndOrAbort();
}

bool ULineTool::MakeObject(const FVector& Location)
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	doc->ClearPreviewDeltas(GetWorld());

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto pendingSegment = dimensionActor->GetLineActor();

	FVector constrainedStartPoint = pendingSegment->Point1;

	if (State == NewSegmentPending && pendingSegment != nullptr &&
		(FVector::Dist(constrainedStartPoint, Location) >= 1.0f))
	{
		GetEdgeDeltas(constrainedStartPoint, Location, false);
	}

	if (!doc->ApplyDeltas(CurDeltas, GetWorld()))
	{
		return false;
	}

	UModumateAnalyticsStatics::RecordObjectCreation(this, EObjectType::OTMetaEdge);

	// Decide whether to end the tool's use, or continue the chain, based on axis constraint
	const FVector& normal = Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal;
	const FVector& tangent = (Location - constrainedStartPoint).GetSafeNormal();
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(Location, normal, tangent);

	if (pendingSegment)
	{
		pendingSegment->Point1 = Location;
		pendingSegment->Point2 = Location;
	}

	return true;
}

bool ULineTool::GetEdgeDeltas(const FVector& StartPosition, const FVector &EndPosition, bool bIsPreview)
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	TArray<int32> addedFaceIDs, addedEdgeIDs, addedVertexIDs;
	return doc->MakeMetaObject(GetWorld(), { StartPosition, EndPosition }, {}, EObjectType::OTMetaEdge, Controller->EMPlayerState->GetViewGroupObjectID(),
		addedVertexIDs, addedEdgeIDs, addedFaceIDs, CurDeltas, !bIsPreview);
}
