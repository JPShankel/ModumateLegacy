// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelStairTool.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

using namespace Modumate;

UStairTool::UStairTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentState(Starting)
	, bWasShowingSnapCursor(false)
	, OriginalMouseMode(EMouseMode::Location)
	, bWantedVerticalSnap(false)
	, LastValidTargetID(MOD_ID_NONE)
	, RunSegmentID(MOD_ID_NONE)
	, RiseSegmentID(MOD_ID_NONE)
	, WidthSegmentID(MOD_ID_NONE)
	, PendingObjMesh(nullptr)
	, GameMode(nullptr)
	, GameState(nullptr)
	, RunStartPos(ForceInitToZero)
	, RiseStartPos(ForceInitToZero)
	, RiseEndPos(ForceInitToZero)
	, WidthEndPos(ForceInitToZero)
	, RunDir(ForceInitToZero)
	, WidthDir(ForceInitToZero)
	, DesiredTreadDepth(30.48f)	// TODO: get from Document
	, CurrentTreadDepth(0.0f)
	, CurrentRiserHeight(0.0f)
	, CurrentWidth(0.0f)
	, DesiredTreadNum(0)
	, CurrentTreadNum(0)
	, CurrentRiserNum(0)
	, bWantStartRiser(true)
	, bWantEndRiser(true)
	, bFixTreadDepth(true)
	, RunSegmentColor(0xFF, 0x80, 0x80)
	, RiseSegmentColor(0x80, 0x80, 0xFF)
	, WidthSegmentColor(0x80, 0xFF, 0x80)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	if (world)
	{
		GameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
		GameState = world->GetGameState<AEditModelGameState_CPP>();
	}
}

bool UStairTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	bWasShowingSnapCursor = Controller->EMPlayerState->bShowSnappedCursor;
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	bWantedVerticalSnap = Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	CurrentState = Starting;

	return true;
}

bool UStairTool::FrameUpdate()
{
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	switch (CurrentState)
	{
	case Starting:
	{
		LastValidTargetID = MOD_ID_NONE;
		const FModumateObjectInstance *hitMOI = nullptr;

		if ((cursor.SnapType == ESnapType::CT_FACESELECT) && cursor.Actor)
		{
			hitMOI = GameState->Document.ObjectFromActor(cursor.Actor);
			if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTStaircase))
			{
				hitMOI = hitMOI->GetParentObject();
			}

			if (ValidatePlaneTarget(hitMOI))
			{
				LastValidTargetID = hitMOI->ID;
			}
		}

		if (LastValidTargetID)
		{
			int32 numCorners = hitMOI->GetNumCorners();
			for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
			{
				int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

				FVector curCornerPos = hitMOI->GetCorner(curCornerIdx);
				FVector nextCornerPos = hitMOI->GetCorner(nextCornerIdx);

				Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(
					curCornerPos, nextCornerPos, TargetPlaneDotColor, TargetPlaneDotInterval, TargetPlaneDotThickness, 1)
				);
			}
		}

		// Don't show the snap cursor if we're targeting a plane.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}
	break;
	case RunPending:
	{
		auto runSegment = DimensionManager->GetDimensionActor(RunSegmentID)->GetLineActor();
		if (cursor.Visible && runSegment != nullptr)
		{
			FVector projectedCursor = cursor.SketchPlaneProject(cursor.WorldPosition);
			FVector rawRunDelta = projectedCursor - RunStartPos;
			float rawRunLength = rawRunDelta.Size();

			runSegment->Point2 = RunStartPos;
			RunDir = FVector::ZeroVector;
			CurrentTreadDepth = 0.0f;
			CurrentTreadNum = 0;

			if (!FMath::IsNearlyZero(rawRunLength))
			{
				RunDir = rawRunDelta / rawRunLength;
				WidthDir = RunDir ^ FVector::UpVector;

				if (bFixTreadDepth && (DesiredTreadDepth > 0.0f))
				{
					CurrentTreadDepth = DesiredTreadDepth;
					CurrentTreadNum = FMath::FloorToInt(rawRunLength / DesiredTreadDepth);
					float quantizedRunLength = DesiredTreadDepth * CurrentTreadNum;
					runSegment->Point2 = RunStartPos + (quantizedRunLength * RunDir);
				}
				else if (!bFixTreadDepth && (DesiredTreadNum > 0))
				{
					CurrentTreadDepth = rawRunLength / DesiredTreadNum;
					CurrentTreadNum = DesiredTreadNum;
					runSegment->Point2 = projectedCursor;
				}
			}
		}
	}
	break;
	case RisePending:
	{
		auto riseSegment = DimensionManager->GetDimensionActor(RiseSegmentID)->GetLineActor();
		if (riseSegment != nullptr && cursor.Visible)
		{
			riseSegment->Point2 = RiseStartPos;
			CurrentRiserHeight = 0.0f;
			CurrentRiserNum = (CurrentTreadNum - 1) +
				(bWantStartRiser ? 1 : 0) +
				(bWantEndRiser ? 1 : 0);

			FVector rawRiseDelta = (cursor.WorldPosition - RiseStartPos);
			float totalRise = rawRiseDelta | FVector::UpVector;

			// TODO: support downward stairs
			if (totalRise > KINDA_SMALL_NUMBER)
			{
				FVector projectedRiseDelta = (totalRise * FVector::UpVector);
				CurrentRiserHeight = totalRise / CurrentRiserNum;

				riseSegment->Point2 = RiseStartPos + projectedRiseDelta;
			}
		}
	}
	break;
	case WidthPending:
	{
		auto widthSegment = DimensionManager->GetDimensionActor(WidthSegmentID)->GetLineActor();
		if (widthSegment != nullptr && cursor.Visible)
		{
			FVector rawWidthDelta = (cursor.WorldPosition - RiseEndPos);
			CurrentWidth = rawWidthDelta | WidthDir;
			FVector projectedWidthDelta = (CurrentWidth * WidthDir);

			widthSegment->Point2 = RiseEndPos + projectedWidthDelta;
		}
	}
	break;
	}

	UpdatePreviewStairs();

	return true;
}

bool UStairTool::Deactivate()
{
	ResetState();

	if (Controller)
	{
		Controller->EMPlayerState->bShowSnappedCursor = bWasShowingSnapCursor;
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = bWantedVerticalSnap;
	}

	return UEditModelToolBase::Deactivate();
}

bool UStairTool::HandleInputNumber(double n)
{
	return false;
}

bool UStairTool::BeginUse()
{
	Super::BeginUse();

	ResetState();

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	PendingObjMesh = Controller->GetWorld()->SpawnActor<ADynamicMeshActor>(GameMode->DynamicMeshActorClass.Get());
	PendingObjMesh->SetActorHiddenInGame(true);
	PendingObjMesh->SetActorEnableCollision(false);

	return EnterNextStage();
}

bool UStairTool::EndUse()
{
	ResetState();

	return UEditModelToolBase::EndUse();
}

bool UStairTool::AbortUse()
{
	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	switch (CurrentState)
	{
	case Starting:
	case RunPending:
	{
		ResetState();
		Super::AbortUse();
	}
	break;
	case RisePending:
	{
		PendingObjMesh->SetActorHiddenInGame(true);
		DimensionManager->ReleaseDimensionActor(RiseSegmentID);
		RiseSegmentID = 0;
		cursor.SetAffordanceFrame(RunStartPos, FVector::UpVector);
		CurrentState = RunPending;
	}
	break;
	case WidthPending:
	{
		DimensionManager->ReleaseDimensionActor(WidthSegmentID);
		WidthSegmentID = 0;
		cursor.SetAffordanceFrame(RiseStartPos, RunDir, WidthDir);
		CurrentWidth = 0.0f;
		CurrentState = RisePending;
	}
	break;
	}

	return true;
}

bool UStairTool::EnterNextStage()
{
	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	if (!cursor.Visible)
	{
		return false;
	}

	switch (CurrentState)
	{
	case Starting:
	{
		if (LastValidTargetID != MOD_ID_NONE)
		{
			bool bCreationSuccess = MakeStairs();
			return !bCreationSuccess;
		}
		else
		{
			RunStartPos = cursor.WorldPosition;
			cursor.SetAffordanceFrame(RunStartPos, FVector::UpVector);
			MakePendingSegment(RunSegmentID, RunStartPos, RunSegmentColor);
			CurrentState = RunPending;
		}
	}
	break;
	case RunPending:
	{
		if (CurrentTreadNum > 0)
		{
			auto runSegment = DimensionManager->GetDimensionActor(RunSegmentID)->GetLineActor();
			RiseStartPos = runSegment->Point2;
			cursor.SetAffordanceFrame(RiseStartPos, RunDir, WidthDir);
			MakePendingSegment(RiseSegmentID, RiseStartPos, RiseSegmentColor);
			PendingObjMesh->SetActorHiddenInGame(false);
			CurrentState = RisePending;
		}
	}
	break;
	case RisePending:
	{
		if (!FMath::IsNearlyZero(CurrentRiserHeight))
		{
			auto riseSegment = DimensionManager->GetDimensionActor(RiseSegmentID)->GetLineActor();
			RiseEndPos = riseSegment->Point2;
			cursor.SetAffordanceFrame(RiseEndPos, RunDir, WidthDir);
			MakePendingSegment(WidthSegmentID, RiseEndPos, WidthSegmentColor);
			CurrentState = WidthPending;
		}
	}
	break;
	case WidthPending:
	{
		if (!FMath::IsNearlyZero(CurrentWidth))
		{
			bool bCreationSuccess = MakeStairs();
			return false;
		}
	}
	break;
	}

	return true;
}

void UStairTool::SetAssemblyKey(const FBIMKey& InAssemblyKey)
{
	UEditModelToolBase::SetAssemblyKey(InAssemblyKey);

	EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(EObjectType::OTStaircase);
	const FBIMAssemblySpec *assembly = GameState.IsValid() ?
		GameState->Document.PresetManager.GetAssemblyByKey(toolMode, InAssemblyKey) : nullptr;

	if (assembly != nullptr)
	{
		ObjAssembly = *assembly;
	}
}

bool UStairTool::UpdatePreviewStairs()
{
	if (!PendingObjMesh.IsValid())
	{
		return false;
	}

	// Don't show the preview stairs until we're in a state that they have enough information to be shown
	switch (CurrentState)
	{
	case Starting:
	case RunPending:
		PendingObjMesh->SetActorHiddenInGame(true);
		return false;
	default:
		break;
	}

	// TODO: get from document, or somewhere else
	float treadThickness = Units::FUnitValue(1.0f, Units::EUnitType::WorldInches).AsWorldCentimeters();
	FVector previewWidthDir = ((CurrentWidth > 0) ? 1 : -1) * WidthDir;
	float previewTotalWidth = FMath::Max(FMath::Abs(CurrentWidth), 0.01f);

	// Calculate the polygons that make up the outer surfaces of each tread and riser of the linear stair run
	bool bStairSuccess = FStairStatics::MakeLinearRunPolysFromBox(
		RunDir, previewWidthDir, CurrentTreadDepth, CurrentRiserHeight, CurrentTreadNum, previewTotalWidth,
		true, bWantStartRiser, bWantEndRiser, CachedTreadPolys, CachedRiserPolys
	);

	// For linear stairs, each riser has the same normal, so populate them here
	int32 numRisers = CachedRiserPolys.Num();
	if (bStairSuccess)
	{
		CachedRiserNormals.SetNum(numRisers);
		for (int32 i = 0; i < numRisers; ++i)
		{
			CachedRiserNormals[i] = RunDir;
		}
	}

	// TODO: get this material from a real assembly
	FArchitecturalMaterial material;
	material.EngineMaterial = PendingObjMesh->StaircaseMaterial;

	// Set up the triangulated staircase mesh by extruding each tread and riser polygon
	bStairSuccess = bStairSuccess && PendingObjMesh->SetupStairPolys(RunStartPos, CachedTreadPolys, CachedRiserPolys, CachedRiserNormals, treadThickness, material);

	PendingObjMesh->SetActorHiddenInGame(!bStairSuccess);

	return bStairSuccess;
}

bool UStairTool::MakeStairs()
{
	TArray<int32> hostPlaneIDs;
	bool bMakePlane = (LastValidTargetID == MOD_ID_NONE);
	bool bSuccess = false;

	if (bMakePlane)
	{
		Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kBeginUndoRedoMacro));

		FVector totalRunDelta = ((CurrentTreadDepth * CurrentTreadNum) * RunDir);
		FVector totalRiseDelta = ((CurrentRiserHeight * CurrentRiserNum) * FVector::UpVector);
		FVector totalWidthDelta = (CurrentWidth * WidthDir);
		TArray<FVector> newPlanePoints({
			RunStartPos,
			RunStartPos + totalWidthDelta,
			RunStartPos + totalWidthDelta + totalRunDelta + totalRiseDelta,
			RunStartPos + totalRunDelta + totalRiseDelta
		});

		TArray<int32> newGraphObjIDs;
		if (GameState->Document.MakeMetaObject(GetWorld(), newPlanePoints, {}, EObjectType::OTMetaPlane, Controller->EMPlayerState->GetViewGroupObjectID(), newGraphObjIDs))
		{
			for (int32 newGraphObjID : newGraphObjIDs)
			{
				auto newGraphObj = GameState->Document.GetObjectById(newGraphObjID);
				if (newGraphObj && (newGraphObj->GetObjectType() == EObjectType::OTMetaPlane))
				{
					hostPlaneIDs.Add(newGraphObjID);
				}
			}
		}
	}
	else
	{
		hostPlaneIDs.Add(LastValidTargetID);
	}

	if (hostPlaneIDs.Num() > 0)
	{
		int32 nextID = GameState->Document.GetNextAvailableID();
		TArray<FMOIStateData> newStairStates;

		for (int32 hostPlaneID : hostPlaneIDs)
		{
			FMOIStateData& newStairState = newStairStates.AddDefaulted_GetRef();
			newStairState.StateType = EMOIDeltaType::Create;
			newStairState.ObjectType = EObjectType::OTStaircase;
			newStairState.ParentID = hostPlaneID;
			newStairState.ObjectAssemblyKey = AssemblyKey;
			newStairState.bObjectInverted = false;
			newStairState.Extents = FVector(0.5f, 0, 0);
			newStairState.ObjectID = nextID++;
		}

		TArray<FDeltaPtr> deltas;
		deltas.Add(MakeShared<FMOIDelta>(newStairStates));

		bSuccess = GameState->Document.ApplyDeltas(deltas, GetWorld());
	}

	if (bMakePlane)
	{
		Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kEndUndoRedoMacro));
	}

	return bSuccess;
}

bool UStairTool::ValidatePlaneTarget(const FModumateObjectInstance *PlaneTarget)
{
	if ((CurrentState != Starting) || (PlaneTarget == nullptr) || (PlaneTarget->GetObjectType() != EObjectType::OTMetaPlane))
	{
		return false;
	}

	// Make sure the normal is not horizontal or vertical
	FVector targetNormal = PlaneTarget->GetNormal();
	float normalUpDot = FMath::Abs(targetNormal | FVector::UpVector);
	return FMath::IsWithinInclusive(normalUpDot, THRESH_NORMALS_ARE_ORTHOGONAL, THRESH_NORMALS_ARE_PARALLEL);
}

void UStairTool::MakePendingSegment(int32 &TargetSegmentID, const FVector &StartingPoint, const FColor &SegmentColor)
{
	auto dimensionActor = DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
	TargetSegmentID = dimensionActor->ID;

	auto segment = dimensionActor->GetLineActor();
	segment->Point1 = StartingPoint;
	segment->Point2 = StartingPoint;
	segment->Color = SegmentColor;
	segment->Thickness = 2;
}

void UStairTool::ResetState()
{
	TArray<int32> pendingSegmentIDs = { RunSegmentID, RiseSegmentID, WidthSegmentID };
	for (int32 &pendingSegmentID : pendingSegmentIDs)
	{
		DimensionManager->ReleaseDimensionActor(pendingSegmentID);
		pendingSegmentID = 0;
	}

	if (PendingObjMesh.IsValid())
	{
		PendingObjMesh->Destroy();
		PendingObjMesh.Reset();
	}

	CurrentState = Starting;
	RunStartPos = FVector::ZeroVector;
	RiseStartPos = FVector::ZeroVector;
	RiseEndPos = FVector::ZeroVector;
	WidthEndPos = FVector::ZeroVector;
	RunDir = FVector::ZeroVector;
	WidthDir = FVector::ZeroVector;
	CurrentTreadDepth = 0.0f;
	CurrentRiserHeight = 0.0f;
	CurrentWidth = 0.0f;
	DesiredTreadNum = 0;
	CurrentTreadNum = 0;
	CurrentRiserNum = 0;
}

