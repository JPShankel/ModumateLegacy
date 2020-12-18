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

	if (PendingObjMesh == nullptr)
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

	if (PendingObjMesh)
	{
		PendingObjMesh->Destroy();
		PendingObjMesh = nullptr;
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
		const AModumateObjectInstance *hitMOI = nullptr;
		LineStartPos = LineEndPos = FVector::ZeroVector;

		if (cursor.Actor)
		{
			hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);

			if (hitMOI && (hitMOI->GetObjectType() == UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode()) ))
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

void UStructureLineTool::OnCreateObjectModeChanged()
{
	Super::OnCreateObjectModeChanged();

	if ((CreateObjectMode == EToolCreateObjectMode::Apply) && IsInUse())
	{
		AbortUse();
	}
	else
	{
		ResetState();
	}
}

void UStructureLineTool::OnAssemblyChanged()
{
	Super::OnAssemblyChanged();

	EToolMode toolMode = GetToolMode();
	const FBIMAssemblySpec* assembly = GameState ?
		GameState->Document->PresetManager.GetAssemblyByKey(toolMode, AssemblyKey) : nullptr;

	if (assembly != nullptr)
	{
		ObjAssembly = *assembly;
		bHaveSetUpGeometry = false;
	}
	else
	{
		AssemblyKey = FBIMKey();
		ObjAssembly = FBIMAssemblySpec();
	}
}

void UStructureLineTool::SetTargetID(int32 NewTargetID)
{
	if (LastValidTargetID != NewTargetID)
	{
		int32 newTargetStructureLineID = MOD_ID_NONE;

		AModumateObjectInstance *targetObj = GameState->Document->GetObjectById(NewTargetID);
		if (targetObj && (targetObj->GetObjectType() == EObjectType::OTMetaEdge))
		{
			// Find a child structure line on the target (that this tool didn't just create)
			TArray<AModumateObjectInstance*> children = targetObj->GetChildObjects();
			for (AModumateObjectInstance *child : children)
			{
				if (child && (child->GetObjectType() == UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode()) ))
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
	if (GameState && (StructureLineID != MOD_ID_NONE))
	{
		AModumateObjectInstance *structureLineObj = GameState->Document->GetObjectById(StructureLineID);
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
	if ((PendingObjMesh == nullptr) || AssemblyKey.IsNone())
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

	FVector2D justification(0.5f, 0.5f);
	PendingObjMesh->SetupExtrudedPolyGeometry(ObjAssembly, LineStartPos, LineEndPos, ObjNormal, ObjUp,
		justification, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector::OneVector, !bHaveSetUpGeometry, false);
	PendingObjMesh->SetActorHiddenInGame(false);

	bHaveSetUpGeometry = true;
	return true;
}

bool UStructureLineTool::MakeStructureLine(int32 TargetEdgeID)
{
	if (GameState == nullptr)
	{
		return false;
	}

	TArray<FDeltaPtr> deltas;
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

		TArray<FVector> points({ LineStartPos, LineEndPos });
		TArray<int32> addedVertexIDs, addedFaceIDs;
		if (!GameState->Document->MakeMetaObject(Controller->GetWorld(), points, {}, EObjectType::OTMetaEdge, Controller->EMPlayerState->GetViewGroupObjectID(),
			addedVertexIDs, targetEdgeIDs, addedFaceIDs, deltas))
		{
			return false;
		}
	}
	else
	{
		targetEdgeIDs.Add(TargetEdgeID);
	}

	TSharedPtr<FMOIDelta> structureLineDelta;
	int32 nextStructureLineID = GameState->Document->GetNextAvailableID();
	for (int32 targetEdgeID : targetEdgeIDs)
	{
		// TODO: fill in custom instance data for StructureLine, once we define and rely on it
		FMOIStateData stateData(nextStructureLineID++,
			UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode()), targetEdgeID);
		stateData.AssemblyKey = AssemblyKey;

		if (!structureLineDelta.IsValid())
		{
			structureLineDelta = MakeShared<FMOIDelta>();
		}
		structureLineDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	}
	if (structureLineDelta.IsValid())
	{
		deltas.Add(structureLineDelta);
	}

	if (deltas.Num() == 0)
	{
		return false;
	}

	return GameState->Document->ApplyDeltas(deltas, GetWorld());
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

UMullionTool::UMullionTool(const FObjectInitializer& ObjectInitializer)
	: UStructureLineTool(ObjectInitializer)
{ }
