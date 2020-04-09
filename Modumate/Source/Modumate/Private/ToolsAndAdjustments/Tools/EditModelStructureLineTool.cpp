// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelStructureLineTool.h"

#include "DynamicMeshActor.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelGameState_CPP.h"
#include "Graph3D.h"
#include "LineActor3D_CPP.h"
#include "ModumateDocument.h"
#include "ModumateCommands.h"
#include "ModumateStairStatics.h"

using namespace Modumate;

UStructureLineTool::UStructureLineTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bHaveSetUpGeometry(false)
	, bWasShowingSnapCursor(false)
	, OriginalMouseMode(EMouseMode::Location)
	, bWantedVerticalSnap(false)
	, LastValidTargetID(MOD_ID_NONE)
	, LastTargetStructureLineID(MOD_ID_NONE)
	, PendingSegment(nullptr)
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
	return false;
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
	if (!Assembly.Valid)
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

		if (!PendingSegment.IsValid())
		{
			PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(AEditModelGameMode_CPP::LineClass);
		}

		PendingSegment->Point1 = LineStartPos;
		PendingSegment->Point2 = LineStartPos;
		PendingSegment->Color = FColor::White;
		PendingSegment->Thickness = 2;
		PendingSegment->Inverted = false;
		PendingSegment->bDrawVerticalPlane = false;
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

	if ((CreateObjectMode == EToolCreateObjectMode::Draw) &&
		PendingSegment.IsValid() && !PendingSegment->Point1.Equals(PendingSegment->Point2))
	{
		bool bCreationSuccess = MakeStructureLine();
		return false;
	}

	return true;
}

bool UStructureLineTool::FrameUpdate()
{
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	if (!cursor.Visible || !Assembly.Valid)
	{
		return false;
	}

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::Draw:
	{
		if (IsInUse() && PendingSegment.IsValid())
		{
			LineEndPos = cursor.WorldPosition;
			PendingSegment->Point2 = LineEndPos;
			LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
			UModumateGeometryStatics::FindBasisVectors(ObjNormal, ObjUp, LineDir);

			if (!LineStartPos.Equals(LineEndPos))
			{
				Controller->UpdateDimensionString(LineStartPos, LineEndPos, ObjNormal);
			}
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

			if (hitMOI && (hitMOI->ObjectType == EObjectType::OTStructureLine))
			{
				hitMOI = hitMOI->GetParentObject();
			}
		}

		if ((hitMOI != nullptr) && (hitMOI->ObjectType == EObjectType::OTMetaEdge))
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
				TargetEdgeDotColor, TargetEdgeDotInterval, TargetEdgeDotThickness, 1)
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

void UStructureLineTool::SetAssembly(const FShoppingItem &key)
{
	Super::SetAssembly(key);

	EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(EObjectType::OTStructureLine);
	const FModumateObjectAssembly *assembly = GameState.IsValid() ?
		GameState->GetAssemblyByKey_DEPRECATED(toolMode, key.Key) : nullptr;

	if (assembly != nullptr)
	{
		ObjAssembly = *assembly;
		bHaveSetUpGeometry = false;
	}
	else
	{
		Assembly = FShoppingItem::ErrorItem;
		ObjAssembly = FModumateObjectAssembly();
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
		if (targetObj && (targetObj->ObjectType == EObjectType::OTMetaEdge))
		{
			// Find a child structure line on the target (that this tool didn't just create)
			TArray<FModumateObjectInstance*> children = targetObj->GetChildObjects();
			for (FModumateObjectInstance *child : children)
			{
				if (child && (child->ObjectType == EObjectType::OTStructureLine))
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
	if (!PendingObjMesh.IsValid() || !Assembly.Valid)
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

	FVector scaleVector;
	if (!ObjAssembly.TryGetProperty(BIM::Parameters::Scale, scaleVector))
	{
		scaleVector = FVector::OneVector;
	}

	PendingObjMesh->SetupExtrudedPolyGeometry(ObjAssembly, LineStartPos, LineEndPos, ObjNormal, ObjUp,
		FVector2D::ZeroVector, FVector2D::ZeroVector, scaleVector, !bHaveSetUpGeometry, false);
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

	for (int32 targetEdgeID : targetEdgeIDs)
	{
		FModumateObjectInstance *parentEdgeObj = GameState->Document.GetObjectById(targetEdgeID);

		if (parentEdgeObj && (parentEdgeObj->ObjectType == EObjectType::OTMetaEdge))
		{
			auto commandResult = Controller->ModumateCommand(
				FModumateCommand(Modumate::Commands::kMakeStructureLine)
				.Param(Parameters::kAssembly, Assembly.Key)
				.Param(Parameters::kParent, targetEdgeID)
			);

			bool bCommandSuccess = commandResult.GetValue(Parameters::kSuccess);
			int32 newStructureLineID = commandResult.GetValue(Parameters::kObjectID);

			if (!bCommandSuccess || (newStructureLineID == MOD_ID_NONE))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to create StructureLine on edge ID %d!"), targetEdgeID);
				bAnyFailure = true;
			}
		}
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
	if (PendingSegment.IsValid())
	{
		PendingSegment->Destroy();
		PendingSegment.Reset();
	}

	bHaveSetUpGeometry = false;
	SetTargetID(MOD_ID_NONE);
	LineStartPos = FVector::ZeroVector;
	LineEndPos = FVector::ZeroVector;
	LineDir = FVector::ZeroVector;
	ObjNormal = FVector::ZeroVector;
	ObjUp = FVector::ZeroVector;
}

