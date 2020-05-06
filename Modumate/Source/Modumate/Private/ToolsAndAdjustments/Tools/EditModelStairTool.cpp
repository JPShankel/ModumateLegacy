// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelStairTool.h"

#include "DynamicMeshActor.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelGameState_CPP.h"
#include "LineActor.h"
#include "ModumateDocument.h"
#include "ModumateCommands.h"
#include "ModumateStairStatics.h"
#include "ModumateFunctionLibrary.h"

using namespace Modumate;

UStairTool::UStairTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentState(Starting)
	, bWasShowingSnapCursor(false)
	, OriginalMouseMode(EMouseMode::Location)
	, bWantedVerticalSnap(false)
	, LastValidTargetID(MOD_ID_NONE)
	, RunSegment(nullptr)
	, RiseSegment(nullptr)
	, WidthSegment(nullptr)
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
			int32 numCorners = hitMOI->GetControlPoints().Num();
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
		if (RunSegment.IsValid() && cursor.Visible)
		{
			FVector projectedCursor = cursor.SketchPlaneProject(cursor.WorldPosition);
			FVector rawRunDelta = projectedCursor - RunStartPos;
			float rawRunLength = rawRunDelta.Size();

			RunSegment->Point2 = RunStartPos;
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
					RunSegment->Point2 = RunStartPos + (quantizedRunLength * RunDir);
				}
				else if (!bFixTreadDepth && (DesiredTreadNum > 0))
				{
					CurrentTreadDepth = rawRunLength / DesiredTreadNum;
					CurrentTreadNum = DesiredTreadNum;
					RunSegment->Point2 = projectedCursor;
				}
			}

			if (CurrentTreadNum > 0)
			{
				Controller->UpdateDimensionString(RunSegment->Point1, RunSegment->Point2, FVector::UpVector);
			}
		}
	}
	break;
	case RisePending:
	{
		if (RunSegment.IsValid() && (CurrentTreadNum > 0))
		{
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				RunSegment->Point1,
				RunSegment->Point2,
				FVector::UpVector,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				0,
				Controller,
				EDimStringStyle::Fixed,
				EEnterableField::NonEditableText,
				0.f,
				EAutoEditableBox::Never);
		}

		if (RiseSegment.IsValid() && cursor.Visible)
		{
			RiseSegment->Point2 = RiseStartPos;
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

				RiseSegment->Point2 = RiseStartPos + projectedRiseDelta;
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller,
					RiseSegment->Point1,
					RiseSegment->Point2,
					RunDir,
					Controller->DimensionStringGroupID_Default,
					Controller->DimensionStringUniqueID_Total,
					0,
					Controller,
					EDimStringStyle::Fixed,
					EEnterableField::NonEditableText,
					0.f,
					EAutoEditableBox::UponUserInput);
			}
		}
	}
	break;
	case WidthPending:
	{
		if (RunSegment.IsValid() && (CurrentTreadNum > 0))
		{
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				RunSegment->Point1,
				RunSegment->Point2,
				FVector::UpVector,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				0,
				Controller,
				EDimStringStyle::Fixed,
				EEnterableField::NonEditableText,
				0.f,
				EAutoEditableBox::Never);
		}

		if (RiseSegment.IsValid() && (CurrentRiserNum > 0))
		{
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				RiseSegment->Point1,
				RiseSegment->Point2,
				FVector::UpVector,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				0,
				Controller,
				EDimStringStyle::Fixed,
				EEnterableField::NonEditableText,
				0.f,
				EAutoEditableBox::Never);
		}

		if (WidthSegment.IsValid() && cursor.Visible)
		{
			FVector rawWidthDelta = (cursor.WorldPosition - RiseEndPos);
			CurrentWidth = rawWidthDelta | WidthDir;
			FVector projectedWidthDelta = (CurrentWidth * WidthDir);
			FVector projectedCursor = RiseEndPos + projectedWidthDelta;

			WidthSegment->Point2 = RiseEndPos;

			if (!FMath::IsNearlyZero(CurrentWidth))
			{
				WidthSegment->Point2 = projectedCursor;
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller,
					WidthSegment->Point1,
					WidthSegment->Point2,
					FVector::UpVector,
					Controller->DimensionStringGroupID_Default,
					Controller->DimensionStringUniqueID_Total,
					0,
					Controller,
					EDimStringStyle::Fixed,
					EEnterableField::NonEditableText,
					0.f,
					EAutoEditableBox::UponUserInput);
			}
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
		if (RiseSegment.IsValid())
		{
			RiseSegment->Destroy();
			RiseSegment.Reset();
		}
		cursor.SetAffordanceFrame(RunStartPos, FVector::UpVector);
		CurrentState = RunPending;
	}
	break;
	case WidthPending:
	{
		if (WidthSegment.IsValid())
		{
			WidthSegment->Destroy();
			WidthSegment.Reset();
		}
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
			int32 newStairsID = MOD_ID_NONE;
			bool bCreationSuccess = MakeStairs(LastValidTargetID, newStairsID);
			return !bCreationSuccess;
		}
		else
		{
			RunStartPos = cursor.WorldPosition;
			cursor.SetAffordanceFrame(RunStartPos, FVector::UpVector);
			MakePendingSegment(RunSegment, RunStartPos, RunSegmentColor);
			CurrentState = RunPending;
		}
	}
	break;
	case RunPending:
	{
		if (CurrentTreadNum > 0)
		{
			RiseStartPos = RunSegment->Point2;
			cursor.SetAffordanceFrame(RiseStartPos, RunDir, WidthDir);
			MakePendingSegment(RiseSegment, RiseStartPos, RiseSegmentColor);
			PendingObjMesh->SetActorHiddenInGame(false);
			CurrentState = RisePending;
		}
	}
	break;
	case RisePending:
	{
		if (!FMath::IsNearlyZero(CurrentRiserHeight))
		{
			RiseEndPos = RiseSegment->Point2;
			cursor.SetAffordanceFrame(RiseEndPos, RunDir, WidthDir);
			MakePendingSegment(WidthSegment, RiseEndPos, WidthSegmentColor);
			CurrentState = WidthPending;
		}
	}
	break;
	case WidthPending:
	{
		if (!FMath::IsNearlyZero(CurrentWidth))
		{
			int32 newStairsID = MOD_ID_NONE;
			bool bCreationSuccess = MakeStairs(LastValidTargetID, newStairsID);
			return false;
		}
	}
	break;
	}

	return true;
}

void UStairTool::SetAssembly(const FShoppingItem &key)
{
	Super::SetAssembly(key);

	EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(EObjectType::OTStaircase);
	const FModumateObjectAssembly *assembly = GameState.IsValid() ?
		GameState->GetAssemblyByKey_DEPRECATED(toolMode, key.Key) : nullptr;

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

bool UStairTool::MakeStairs(int32 &RefParentPlaneID, int32 &OutStairsID)
{
	bool bMakePlane = (RefParentPlaneID == MOD_ID_NONE);
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

		auto commandResult = Controller->ModumateCommand(
			FModumateCommand(Commands::kMakeMetaPlane)
			.Param(Parameters::kControlPoints, newPlanePoints)
			.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID()));

		if (!commandResult.GetValue(Parameters::kSuccess))
		{
			return false;
		}

		TArray<int32> newPlaneIDs = commandResult.GetValue(Parameters::kObjectIDs);
		if (newPlaneIDs.Num() != 1)
		{
			return false;
		}
		else
		{
			RefParentPlaneID = newPlaneIDs[0];
		}
	}

	OutStairsID = MOD_ID_NONE;

	if (RefParentPlaneID != MOD_ID_NONE)
	{
		FModumateObjectInstance *parentPlaneObj = GameState->Document.GetObjectById(RefParentPlaneID);

		if (parentPlaneObj && (parentPlaneObj->GetObjectType() == EObjectType::OTMetaPlane))
		{
			OutStairsID = Controller->ModumateCommand(
				FModumateCommand(Modumate::Commands::kMakeMetaPlaneHostedObj)
				.Param(Parameters::kObjectType, EnumValueString(EObjectType, EObjectType::OTStaircase))
				.Param(Parameters::kParent, RefParentPlaneID)
				.Param(Parameters::kAssembly, Assembly.Key)
				.Param(Parameters::kOffset, 0.5f)
				.Param(Parameters::kInverted, false)
			).GetValue(Parameters::kObjectID);
		}
	}

	if (bMakePlane)
	{
		Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kEndUndoRedoMacro));
	}

	return (RefParentPlaneID != MOD_ID_NONE) && (OutStairsID != MOD_ID_NONE);
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

void UStairTool::MakePendingSegment(TWeakObjectPtr<ALineActor> &TargetSegment, const FVector &StartingPoint, const FColor &SegmentColor)
{
	TargetSegment = Controller->GetWorld()->SpawnActor<ALineActor>();
	TargetSegment->Point1 = StartingPoint;
	TargetSegment->Point2 = StartingPoint;
	TargetSegment->Color = SegmentColor;
	TargetSegment->Thickness = 2;
}

void UStairTool::ResetState()
{
	TWeakObjectPtr<ALineActor> pendingSegments[] = { RunSegment, RiseSegment, WidthSegment };
	for (auto &pendingSegment : pendingSegments)
	{
		if (pendingSegment.IsValid())
		{
			pendingSegment->Destroy();
			pendingSegment.Reset();
		}
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

