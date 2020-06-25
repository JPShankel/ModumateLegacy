// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCabinetTool.h"

#include "Algo/Transform.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateObjectInstanceCabinets.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"

using namespace Modumate;

UCabinetTool::UCabinetTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
	, PendingSegmentID(MOD_ID_NONE)
{
	Controller->EMPlayerState;
	CabinetPlane = FPlane(FVector::UpVector, 0.0f);
}

bool UCabinetTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

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

	const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector);

	State = NewSegmentPending;

	auto dimensionActor = GameInstance->DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
	PendingSegmentID = dimensionActor->ID;

	auto pendingSegment = dimensionActor->GetLineActor();
	pendingSegment->Point1 = hitLoc;
	pendingSegment->Point2 = hitLoc;
	pendingSegment->Color = FColor::Green;
	pendingSegment->Thickness = 2;

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

	if (State == NewSegmentPending && PendingSegmentID != MOD_ID_NONE)
	{
		auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();

		FVector sketchPlanePoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(hitLoc);
		if (!sketchPlanePoint.Equals(hitLoc))
		{
			hitLoc = sketchPlanePoint;
			FAffordanceLine affordance;
			affordance.Color = FLinearColor::Blue;
			affordance.EndPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			affordance.StartPoint = hitLoc;
			affordance.Interval = 4.0f;
			Controller->EMPlayerState->AffordanceLines.Add(affordance);
		}

		LastPendingSegmentLoc = hitLoc;
		LastPendingSegmentLocValid = true;

		pendingSegment->Point2 = hitLoc;
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
		auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
		pendingSegment->Point1 = ConnectSegs[0]->Point1;
		pendingSegment->Point2 = ConnectSegs[0]->Point2;
	}

	return true;
}

bool UCabinetTool::HandleInputNumber(double n)
{
	if (State == NewSegmentPending && PendingSegmentID != MOD_ID_NONE)
	{
		auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
		FVector direction = (pendingSegment->Point2 - pendingSegment->Point1).GetSafeNormal();
		FVector origin = pendingSegment->Point1 + direction * n;
		pendingSegment->Point2 = origin;
		MakeSegment(origin);
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
	if (State == SetHeight)
	{
		for (auto &seg : TopSegs)
		{
			seg->Destroy();
		}

		for (auto &seg : ConnectSegs)
		{
			seg->Destroy();
		}

		ConnectSegs.Empty();
		TopSegs.Empty();

		State = NewSegmentPending;
	} 

	// even if the state was set height, the most recent base segment should be reverted as well
	if (BaseSegs.Num() == 0)
	{
		return EndUse();
	}
	
	auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
	auto poppedSegment = BaseSegs.Pop();
	pendingSegment->Point1 = poppedSegment->Point1;
	pendingSegment->Point2 = poppedSegment->Point2;
	pendingSegment->Color = poppedSegment->Color;
	pendingSegment->Thickness = poppedSegment->Thickness;

	poppedSegment->Destroy();

	BasePoints.Pop();

	return true;
}

bool UCabinetTool::EndUse()
{
	Super::EndUse();

	GameInstance->DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	PendingSegmentID = MOD_ID_NONE;

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
	BasePoints.Reset();

	State = Neutral;
	LastPendingSegmentLocValid = false;

	return true;
}

void UCabinetTool::MakeSegment(const FVector &hitLoc)
{
	auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
	if (pendingSegment == nullptr)
	{
		return;
	}

	// TODO: copy constructor(?)
	auto baseSegment = GetWorld()->SpawnActor<ALineActor>();
	baseSegment->Point1 = pendingSegment->Point1;
	baseSegment->Point2 = pendingSegment->Point2;
	baseSegment->Color = pendingSegment->Color;
	baseSegment->Thickness = pendingSegment->Thickness;

	// update base stacks
	BaseSegs.Push(baseSegment);
	BasePoints.Push(baseSegment->Point1);

	bool bClosedLoop = BaseSegs[0]->Point1.Equals(BaseSegs[BaseSegs.Num() - 1]->Point2);
	if (bClosedLoop && UModumateGeometryStatics::IsPolygonValid(BasePoints))
	{
		BeginSetHeightMode(BasePoints);
	}

	pendingSegment->Point1 = hitLoc;
	pendingSegment->Point2 = hitLoc;
}

bool UCabinetTool::EnterNextStage()
{
	Super::EnterNextStage();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	if (State == NewSegmentPending && PendingSegmentID != MOD_ID_NONE)
	{
		auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, (pendingSegment->Point1 - pendingSegment->Point2).GetSafeNormal());
		MakeSegment(hitLoc);

		return true;
	}
	if (State == SetHeight)
	{
		FModumateDocument &doc = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

		float h = TopSegs[0]->Point1.Z - BaseSegs[0]->Point1.Z;

		FMOIStateData stateData;
		stateData.StateType = EMOIDeltaType::Create;
		stateData.ObjectType = EObjectType::OTCabinet;
		stateData.ObjectAssemblyKey = Assembly.Key;
		stateData.ControlPoints = BasePoints;
		stateData.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
		stateData.ObjectID = doc.GetNextAvailableID();
		stateData.Extents = FVector(0, h, 0);

		TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta({ stateData }));
		Controller->ModumateCommand(delta->AsCommand());
		
		return false;
	}
	return false;
}

void UCabinetTool::GetSnappingPointsAndLines(TArray<FVector> &OutPoints, TArray<TPair<FVector, FVector>> &OutLines)
{
	OutPoints = BasePoints;

	for (int32 idx = 0; idx < BasePoints.Num() - 1; idx++)
	{
		OutLines.Add(TPair<FVector, FVector>(BasePoints[idx], BasePoints[idx + 1]));
	}

	// if there is currently a closed loop, create the last line to close the loop
	if (State == SetHeight)
	{
		OutLines.Add(TPair<FVector, FVector>(BasePoints[BasePoints.Num() - 1], BasePoints[0]));
	}
}

void UCabinetTool::BeginSetHeightMode(const TArray<FVector> &basePoly)
{
	Super::BeginUse();

	for (int i = 0; i < basePoly.Num(); ++i)
	{
		auto actor = Controller->GetWorld()->SpawnActor<ALineActor>();
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
