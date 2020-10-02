// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelStructureLineTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/LineActor.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"

using namespace Modumate;

UStructureLineTool::UStructureLineTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bHaveSetUpGeometry(false)
	, bWasShowingSnapCursor(false)
	, OriginalMouseMode(EMouseMode::Location)
	, bWantedVerticalSnap(false)
	, LastValidTargetID(MOD_ID_NONE)
	, LastTargetStructureLineID(MOD_ID_NONE)
	, PendingObjMesh(nullptr)
	, GameMode(nullptr)
	, GameState(nullptr)
	, LineStartPos(ForceInitToZero)
	, LineEndPos(ForceInitToZero)
	, LineDir(ForceInitToZero)
	, ObjNormal(ForceInitToZero)
	, ObjUp(ForceInitToZero)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	if (world)
	{
		GameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
		GameState = world->GetGameState<AEditModelGameState_CPP>();
	}
}

bool UStructureLineTool::Activate()
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
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	ResetState();

	if (!PendingObjMesh.IsValid())
	{
		PendingObjMesh = Controller->GetWorld()->SpawnActor<ADynamicMeshActor>(GameMode->DynamicMeshActorClass.Get());
	}

	PendingObjMesh->SetActorHiddenInGame(true);
	PendingObjMesh->SetActorEnableCollision(false);

	return true;
}

bool UStructureLineTool::HandleInputNumber(double n)
{
	FVector direction = LineEndPos - LineStartPos;
	direction.Normalize();

	LineEndPos = LineStartPos + direction * n;

	if (MakeStructureLine())
	{
		EndUse();
	}

	return true;
}

bool UStructureLineTool::Deactivate()
{
	ResetState();

	if (PendingObjMesh.IsValid())
	{
		PendingObjMesh->Destroy();
		PendingObjMesh.Reset();
	}

	if (Controller)
	{
		Controller->EMPlayerState->bShowSnappedCursor = bWasShowingSnapCursor;
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = bWantedVerticalSnap;
	}

	return UEditModelToolBase::Deactivate();
}

bool UStructureLineTool::BeginUse()
{
	if (AssemblyKey.IsNone())
	{
		return false;
	}

	Super::BeginUse();

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::Draw:
	{
		LineStartPos = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		LineEndPos = LineStartPos;

		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);

		if (dimensionActor != nullptr)
		{
			auto pendingSegment = dimensionActor->GetLineActor();
			pendingSegment->Point1 = LineStartPos;
			pendingSegment->Point2 = LineStartPos;
			pendingSegment->Color = FColor::Black;
			pendingSegment->Thickness = 3;
		}
	}
	break;
	case EToolCreateObjectMode::Apply:
	{
		if (LastValidTargetID != MOD_ID_NONE)
		{
			MakeStructureLine(LastValidTargetID);
		}

		EndUse();
	}
	break;
	}

	return true;
}

bool UStructureLineTool::EnterNextStage()
{
	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	auto pendingSegment = DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
	if ((CreateObjectMode == EToolCreateObjectMode::Draw) &&
		pendingSegment != nullptr && !pendingSegment->Point1.Equals(pendingSegment->Point2))
	{
		bool bCreationSuccess = MakeStructureLine();
		return false;
	}

	return true;
}

bool UStructureLineTool::FrameUpdate()
{
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	if (!cursor.Visible || AssemblyKey.IsNone())
	{
		return false;
	}

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::Draw:
	{
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		if (IsInUse() && dimensionActor != nullptr)
		{
			auto pendingSegment = dimensionActor->GetLineActor();
			LineEndPos = cursor.WorldPosition;
			pendingSegment->Point2 = LineEndPos;
			LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
			UModumateGeometryStatics::FindBasisVectors(ObjNormal, ObjUp, LineDir);
		}
	}
	break;
	case EToolCreateObjectMode::Apply:
	{
		int32 newTargetID = MOD_ID_NONE;
		const FModumateObjectInstance *hitMOI = nullptr;
		LineStartPos = LineEndPos = FVector::ZeroVector;

		if (cursor.Actor)
		{
			hitMOI = GameState->Document.ObjectFromActor(cursor.Actor);

			if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTStructureLine))
			{
				hitMOI = hitMOI->GetParentObject();
			}
		}

		if ((hitMOI != nullptr) && (hitMOI->GetObjectType() == EObjectType::OTMetaEdge))
		{
			newTargetID = hitMOI->ID;
		}

		SetTargetID(newTargetID);
		if (LastValidTargetID != MOD_ID_NONE)
		{
			LineStartPos = hitMOI->GetCorner(0);
			LineEndPos = hitMOI->GetCorner(1);
			LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
			UModumateGeometryStatics::FindBasisVectors(ObjNormal, ObjUp, LineDir);

			Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(
				LineStartPos, LineEndPos,
				AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness, 1)
			);
		}

		// Don't show the snap cursor if we're targeting an edge or existing structure line.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}
	break;
	}

	UpdatePreviewStructureLine();

	return true;
}

bool UStructureLineTool::EndUse()
{
	ResetState();
	return UEditModelToolBase::EndUse();
}

bool UStructureLineTool::AbortUse()
{
	ResetState();
	return UEditModelToolBase::AbortUse();
}

void UStructureLineTool::SetAssemblyKey(const FBIMKey& InAssemblyKey)
{
	Super::SetAssemblyKey(InAssemblyKey);

	EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(EObjectType::OTStructureLine);
	const FBIMAssemblySpec *assembly = GameState.IsValid() ?
		GameState->Document.PresetManager.GetAssemblyByKey(toolMode, InAssemblyKey) : nullptr;

	if (assembly != nullptr)
	{
		ObjAssembly = *assembly;
		bHaveSetUpGeometry = false;
	}
	else
	{
		Super::SetAssemblyKey(FBIMKey());
		ObjAssembly = FBIMAssemblySpec();
	}
}

void UStructureLineTool::SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode)
{
	Super::SetCreateObjectMode(InCreateObjectMode);

	if ((CreateObjectMode == EToolCreateObjectMode::Apply) && IsInUse())
	{
		AbortUse();
	}
	else
	{
		ResetState();
	}
}

void UStructureLineTool::SetTargetID(int32 NewTargetID)
{
	if (LastValidTargetID != NewTargetID)
	{
		int32 newTargetStructureLineID = MOD_ID_NONE;

		FModumateObjectInstance *targetObj = GameState->Document.GetObjectById(NewTargetID);
		if (targetObj && (targetObj->GetObjectType() == EObjectType::OTMetaEdge))
		{
			// Find a child structure line on the target (that this tool didn't just create)
			TArray<FModumateObjectInstance*> children = targetObj->GetChildObjects();
			for (FModumateObjectInstance *child : children)
			{
				if (child && (child->GetObjectType() == EObjectType::OTStructureLine))
				{
					newTargetStructureLineID = child->ID;
					break;
				}
			}
		}

		// Hide it, so we can preview the new line in its place,
		// and unhide the previous target structure line (if there was one)
		if (LastTargetStructureLineID != newTargetStructureLineID)
		{
			SetStructureLineHidden(LastTargetStructureLineID, false);
			SetStructureLineHidden(newTargetStructureLineID, true);
			LastTargetStructureLineID = newTargetStructureLineID;
		}

		LastValidTargetID = NewTargetID;
	}
}

bool UStructureLineTool::SetStructureLineHidden(int32 StructureLineID, bool bHidden)
{
	if (GameState.IsValid() && (StructureLineID != MOD_ID_NONE))
	{
		FModumateObjectInstance *structureLineObj = GameState->Document.GetObjectById(StructureLineID);
		if (structureLineObj)
		{
			static const FName hideRequestTag(TEXT("StructureLineTool"));
			structureLineObj->RequestHidden(hideRequestTag, bHidden);
			return true;
		}
	}

	return false;
}

bool UStructureLineTool::UpdatePreviewStructureLine()
{
	if (!PendingObjMesh.IsValid() || AssemblyKey.IsNone())
	{
		return false;
	}

	if (LineStartPos.Equals(LineEndPos))
	{
		PendingObjMesh->SetActorHiddenInGame(true);
		return false;
	}

	// Set up the extruded polygon for the structure line,
	// and recreate geometry based on whether it's been set up before.

	PendingObjMesh->SetupExtrudedPolyGeometry(ObjAssembly, LineStartPos, LineEndPos, ObjNormal, ObjUp,
		FVector2D::ZeroVector, FVector2D::ZeroVector, FVector::OneVector, !bHaveSetUpGeometry, false);
	PendingObjMesh->SetActorHiddenInGame(false);

	bHaveSetUpGeometry = true;
	return true;
}

bool UStructureLineTool::MakeStructureLine(int32 TargetEdgeID)
{
	if (!GameState.IsValid())
	{
		return false;
	}

	TArray<int32> targetEdgeIDs;
	bool bMakeEdge = (TargetEdgeID == MOD_ID_NONE);
	if (bMakeEdge)
	{
		// Make sure the pending line is valid
		FVector lineDelta = LineEndPos - LineStartPos;
		float lineLength = lineDelta.Size();
		if (FMath::IsNearlyZero(lineLength))
		{
			return false;
		}
		FVector lineDir = lineDelta / lineLength;

		// Start the full macro, since we will be creating meta edge(s) *and* StructureLine object(s)
		Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kBeginUndoRedoMacro));

		TArray<FVector> points({ LineStartPos, LineEndPos });

		auto commandResult = Controller->ModumateCommand(
			FModumateCommand(Commands::kMakeMetaEdge)
			.Param(Parameters::kControlPoints, points)
			.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID()));

		bool bCommandSuccess = commandResult.GetValue(Parameters::kSuccess);
		TArray<int32> newEdgeIDs = commandResult.GetValue(Parameters::kObjectIDs);

		// For each edge that the graph command created that is contained within the pending line,
		// keep track of the edge as a target edge, on which to create a StructureLine object.
		const FGraph3D &volumeGraph = GameState->Document.GetVolumeGraph();

		for (int32 newEdgeID : newEdgeIDs)
		{
			const FGraph3DEdge *newEdge = volumeGraph.FindEdge(newEdgeID);
			if (newEdge)
			{
				const FGraph3DVertex *startVertex = volumeGraph.FindVertex(newEdge->StartVertexID);
				const FGraph3DVertex *endVertex = volumeGraph.FindVertex(newEdge->EndVertexID);
				if (startVertex && endVertex)
				{
					static const float lineEpsilon = RAY_INTERSECT_TOLERANCE;

					float startDistAlongLine = (startVertex->Position - LineStartPos) | LineDir;
					float endDistAlongLine = (endVertex->Position - LineStartPos) | LineDir;

					// Make sure the start and end vertices of the edge in question lie on the pending line segment.
					if (FMath::IsWithinInclusive(startDistAlongLine, -lineEpsilon, lineLength + lineEpsilon) &&
						FMath::IsWithinInclusive(endDistAlongLine, -lineEpsilon, lineLength + lineEpsilon) &&
						startVertex->Position.Equals(LineStartPos + (startDistAlongLine * LineDir)) &&
						endVertex->Position.Equals(LineStartPos + (endDistAlongLine * LineDir)))
					{
						targetEdgeIDs.Add(newEdgeID);
					}
				}
			}
		}
	}
	else
	{
		targetEdgeIDs.Add(TargetEdgeID);
	}

	bool bAnyFailure = false;

	TSharedPtr<FMOIDelta> structureLineDelta;

	for (int32 targetEdgeID : targetEdgeIDs)
	{
		FModumateObjectInstance *parentEdgeObj = GameState->Document.GetObjectById(targetEdgeID);

		if (parentEdgeObj && (parentEdgeObj->GetObjectType() == EObjectType::OTMetaEdge))
		{
			// TODO: fill in custom instance data for stairs, once we define and rely on it
			FMOIStateData stateData(GameState->Document.GetNextAvailableID(), EObjectType::OTStructureLine, targetEdgeID);
			stateData.AssemblyKey = AssemblyKey;

			if (!structureLineDelta.IsValid())
			{
				structureLineDelta = MakeShared<FMOIDelta>();
			}
			structureLineDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
		}
	}

	if (!GameState->Document.ApplyDeltas({ structureLineDelta }, GetWorld()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create StructureLines!"));
		bAnyFailure = true;
	}

	Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kEndUndoRedoMacro));

	// We failed to create a structure line, so undo the operation and exit here.
	if (bAnyFailure)
	{
		GameState->Document.Undo(Controller->GetWorld());
		return false;
	}

	// Otherwise, we're done!
	return true;
}

void UStructureLineTool::ResetState()
{
	bHaveSetUpGeometry = false;
	SetTargetID(MOD_ID_NONE);
	LineStartPos = FVector::ZeroVector;
	LineEndPos = FVector::ZeroVector;
	LineDir = FVector::ZeroVector;
	ObjNormal = FVector::ZeroVector;
	ObjUp = FVector::ZeroVector;
}

