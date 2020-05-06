// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelCabinetTool.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "LineActor.h"
#include "ModumateCommands.h"
#include "Algo/Transform.h"
#include "ModumateObjectInstanceCabinets.h"
#include "ModumateFunctionLibrary.h"

using namespace Modumate;

UCabinetTool::UCabinetTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
{
	Controller->EMPlayerState;
	CabinetPlane = FPlane(FVector::UpVector, 0.0f);
}

bool UCabinetTool::Activate()
{
	Super::Activate();
	Controller->DeselectAll();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	return true;
}

bool UCabinetTool::BeginUse()
{
	Super::BeginUse();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector);

	State = NewSegmentPending;

	PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor>();
	PendingSegment->Point1 = hitLoc;
	PendingSegment->Point2 = hitLoc;
	PendingSegment->Color = FColor::Green;
	PendingSegment->Thickness = 2;

	LastPendingSegmentLoc = hitLoc;
	LastPendingSegmentLocValid = true;

	CabinetPlane = FPlane(hitLoc, FVector::UpVector);

	return true;
}

bool UCabinetTool::FrameUpdate()
{
	Super::FrameUpdate();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	const FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	FVector hitLoc = snappedCursor.WorldPosition;

	if (State == NewSegmentPending && PendingSegment != nullptr)
	{
		FAffordanceLine affordance;
		if (Controller->EMPlayerState->SnappedCursor.TryMakeAffordanceLineFromCursorToSketchPlane(affordance, hitLoc))
		{
			Controller->EMPlayerState->AffordanceLines.Add(affordance);
		}

		LastPendingSegmentLoc = hitLoc;
		LastPendingSegmentLocValid = true;

		PendingSegment->Point2 = hitLoc;
		// TODO: non-horizontal dimensions
		Controller->UpdateDimensionString(PendingSegment->Point1, PendingSegment->Point2, FVector::UpVector);
	}

	if (State == SetHeight)
	{
		FVector verticalPlaneOrigin = LastPendingSegmentLocValid ? LastPendingSegmentLoc : BaseSegs.Last()->Point2;
		FVector projectedDeltaPos = hitLoc;

		Controller->EMPlayerState->GetSnapCursorDeltaFromRay(verticalPlaneOrigin, CabinetPlane, projectedDeltaPos);
		float newHeight = projectedDeltaPos | CabinetPlane;

		FVector avgLocation(ForceInitToZero);
		for (auto &seg : TopSegs)
		{
			seg->Point1.Z = newHeight;
			seg->Point2.Z = newHeight;
		}
		for (auto &seg : ConnectSegs)
		{
			seg->Point2.Z = newHeight;
			avgLocation = (avgLocation + seg->Point2) / 2.f;
		}

		avgLocation.Z = 0.f;
		FVector newHeightLocation = avgLocation;
		newHeightLocation.Z = newHeight;

		FVector camDir = Controller->PlayerCameraManager->GetCameraRotation().Vector();
		camDir.Z = 0.f;
		camDir = camDir.RotateAngleAxis(-90.f, FVector::UpVector);
		// Dim string for cabinet height - Delta Only
		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			avgLocation,
			newHeightLocation,
			camDir,
			Controller->DimensionStringGroupID_PlayerController,
			Controller->DimensionStringUniqueID_Delta,
			0,
			Controller,
			EDimStringStyle::Fixed);
	}

	return true;
}

bool UCabinetTool::HandleInputNumber(double n)
{
	if (State == NewSegmentPending && PendingSegment != nullptr)
	{
		FVector direction = (PendingSegment->Point2 - PendingSegment->Point1).GetSafeNormal();
		FVector origin = PendingSegment->Point1 + direction * n;
		HandleClick(origin);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(origin, direction);
	}
	if (State == SetHeight)
	{
		if (TopSegs[0]->Point1.Z > BaseSegs[0]->Point1.Z)
		{
			TopSegs[0]->Point1.Z = BaseSegs[0]->Point1.Z + n;
		}
		else
		{
			TopSegs[0]->Point1.Z = BaseSegs[0]->Point1.Z - n;
		}
		EnterNextStage();
		EndUse();
	}
	return true;
}

bool UCabinetTool::AbortUse()
{
	Super::AbortUse();
	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
		PendingSegment = nullptr;
	}

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	for (auto &seg : BaseSegs)
	{
		DoMakeLineSegmentCommand(seg->Point1, seg->Point2);
	}

	for (auto &seg : BaseSegs)
	{
		seg->Destroy();
	}

	for (auto &seg : TopSegs)
	{
		seg->Destroy();
	}

	for (auto &seg : ConnectSegs)
	{
		seg->Destroy();
	}

	ConnectSegs.Empty();
	BaseSegs.Empty();
	TopSegs.Empty();
	State = Neutral;
	LastPendingSegmentLocValid = false;

	return true;
}

bool UCabinetTool::EndUse()
{
	Super::EndUse();
	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
		PendingSegment = nullptr;
	}

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	for (auto &seg : BaseSegs)
	{
		seg->Destroy();
	}

	for (auto &seg : TopSegs)
	{
		seg->Destroy();
	}

	for (auto &seg : ConnectSegs)
	{
		seg->Destroy();
	}

	ConnectSegs.Empty();
	BaseSegs.Empty();
	TopSegs.Empty();

	State = Neutral;
	LastPendingSegmentLocValid = false;

	return true;
}

void UCabinetTool::HandleClick(const FVector &p)
{
	if (State == NewSegmentPending && PendingSegment != nullptr)
	{
		DoMakeLineSegmentCommand(p, PendingSegment->Point1);

		if (PendingSegment != nullptr)
		{
			FVector p1 = PendingSegment->Point1;
			FVector p2 = PendingSegment->Point2;

			PendingSegment->Point1 = p;
			PendingSegment->Point2 = p;
			Controller->TryMakeCabinetFromSegments();
		}
	}
}

bool UCabinetTool::EnterNextStage()
{
	Super::EnterNextStage();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	if (State == NewSegmentPending)
	{
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, (PendingSegment->Point1 - PendingSegment->Point2).GetSafeNormal());
		HandleClick(hitLoc);

		return true;
	}
	if (State == SetHeight)
	{
		TArray<FVector> points;
		Algo::Transform(BaseSegs,points,[](const ALineActor *seg) {return seg->Point1; });

		FModumateDocument &doc = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

		float h = TopSegs[0]->Point1.Z - BaseSegs[0]->Point1.Z;

		FMOIStateData stateData;
		stateData.ObjectType = EObjectType::OTCabinet;
		stateData.ObjectAssemblyKey = Assembly.Key;
		stateData.ControlPoints = points;
		stateData.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
		stateData.ObjectID = doc.GetNextAvailableID();
		stateData.Extents = FVector(0, h, 0);

		FMOIDelta delta = FMOIDelta::MakeCreateObjectDelta(stateData);

		Controller->ModumateCommand(delta.AsCommand());
		
		return false;
	}
	return false;
}

void UCabinetTool::BeginSetHeightMode(const TArray<FVector> &basePoly)
{
	Super::BeginUse();

	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
		PendingSegment = nullptr;
	}

	for (int i = 0; i < basePoly.Num(); ++i)
	{
		ALineActor *actor = Controller->GetWorld()->SpawnActor<ALineActor>();
		actor->Point1 = basePoly[i];
		actor->Point2 = basePoly[(i + 1) % basePoly.Num()];
		BaseSegs.Add(actor);

		actor = Controller->GetWorld()->SpawnActor<ALineActor>();
		actor->Point1 = basePoly[i];
		actor->Point2 = basePoly[(i + 1) % basePoly.Num()];
		TopSegs.Add(actor);

		actor = Controller->GetWorld()->SpawnActor<ALineActor>();
		actor->Point1 = basePoly[i];
		actor->Point2 = basePoly[i];
		ConnectSegs.Add(actor);
	}

	if (ensureAlways(basePoly.Num() > 0))
	{
		CabinetPlane = FPlane(basePoly[0], FVector::UpVector);
	}

	const FSnappedCursor& SnappedCursor = Controller->EMPlayerState->SnappedCursor;
	if (SnappedCursor.Visible &&
		SnappedCursor.SnapType != ESnapType::CT_NOSNAP)
	{
		LastPendingSegmentLoc = SnappedCursor.WorldPosition;
		LastPendingSegmentLocValid = true;
	}

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	State = SetHeight;
}

void UCabinetTool::DoMakeLineSegmentCommand(const FVector &P1, const FVector &P2)
{
	FModumateDocument &doc = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

	FMOIStateData state;

	state.ControlPoints = { P1, P2 };
	state.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
	state.ObjectType = EObjectType::OTLineSegment;
	state.ObjectID = doc.GetNextAvailableID();

	FMOIDelta delta = FMOIDelta::MakeCreateObjectDelta(state);

	Controller->ModumateCommand(delta.AsCommand());
}
