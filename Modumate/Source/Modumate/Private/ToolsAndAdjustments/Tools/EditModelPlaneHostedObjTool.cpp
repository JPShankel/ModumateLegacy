// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelPlaneHostedObjTool.h"

#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "LineActor.h"
#include "ModumateDocument.h"
#include "ModumateCommands.h"

using namespace Modumate;

UPlaneHostedObjTool::UPlaneHostedObjTool(const FObjectInitializer& ObjectInitializer)
	: UMetaPlaneTool(ObjectInitializer)
	, PendingObjMesh(nullptr)
	, bInverted(false)
	, bRequireHoverMetaPlane(false)
	, ObjectType(EObjectType::OTNone)
	, LastValidTargetID(MOD_ID_NONE)
	, bWasShowingSnapCursor(true)
{
}

bool UPlaneHostedObjTool::ValidatePlaneTarget(const FModumateObjectInstance *PlaneTarget)
{
	return (PlaneTarget != nullptr) && (PlaneTarget->GetObjectType() == EObjectType::OTMetaPlane);
}

float UPlaneHostedObjTool::GetDefaultJustificationValue()
{
	if (GameState == nullptr)
	{
		return 0.5f;
	}

	if (ObjectType == EObjectType::OTFloorSegment 
		|| ObjectType == EObjectType::OTStaircase)
	{
		return GameState->Document.GetDefaultJustificationXY();
	}
		
	return GameState->Document.GetDefaultJustificationZ();
}

bool UPlaneHostedObjTool::Activate()
{
	if (!UMetaPlaneTool::Activate())
	{
		return false;
	}

	bWasShowingSnapCursor = Controller->EMPlayerState->bShowSnappedCursor;
	return true;
}

bool UPlaneHostedObjTool::FrameUpdate()
{
	if (!UMetaPlaneTool::FrameUpdate())
	{
		return false;
	}

	if (PendingObjMesh.IsValid() && PendingSegment.IsValid())
	{
		bool bSegmentValid = false;
		switch (AxisConstraint)
		{
		case EAxisConstraint::AxisZ:
			bSegmentValid = !PendingSegment->Point1.Equals(PendingSegment->Point2) &&
				FMath::IsNearlyEqual(PendingSegment->Point1.Z, PendingSegment->Point2.Z);// , KINDA_SMALL_NUMBER);
			break;
		case EAxisConstraint::AxesXY:
			bSegmentValid = !FMath::IsNearlyEqual(PendingSegment->Point1.X, PendingSegment->Point2.X) &&
				!FMath::IsNearlyEqual(PendingSegment->Point1.Y, PendingSegment->Point2.Y) &&
				FMath::IsNearlyEqual(PendingSegment->Point1.Z, PendingSegment->Point2.Z);// , KINDA_SMALL_NUMBER);
			break;
		default:
			bSegmentValid = true;
			break;
		}

		if ((PendingPlanePoints.Num() > 0) && bSegmentValid)
		{
			bool bRecreatingGeometry = (PendingObjMesh->LayerGeometries.Num() == 0);
			PendingObjMesh->CreateBasicLayerDefs(PendingPlanePoints, FVector::ZeroVector, ObjAssembly, GetDefaultJustificationValue());
			PendingObjMesh->UpdatePlaneHostedMesh(bRecreatingGeometry, false, false, PendingSegment->Point1);
			PendingObjMesh->SetActorHiddenInGame(false);
		}
		else
		{
			PendingObjMesh->SetActorHiddenInGame(true);
			PendingSegment->SetActorHiddenInGame(true);
		}
	}
	else
	{
		// Determine whether we can apply the plane hosted object to a plane targeted by the cursor
		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		LastValidTargetID = MOD_ID_NONE;
		const FModumateObjectInstance *hitMOI = nullptr;

		if ((cursor.SnapType == ESnapType::CT_FACESELECT) && cursor.Actor)
		{
			hitMOI = GameState->Document.ObjectFromActor(cursor.Actor);
			if (hitMOI && (hitMOI->GetObjectType() == ObjectType))
			{
				hitMOI = GameState->Document.GetObjectById(hitMOI->GetParentID());
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
					curCornerPos, nextCornerPos, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness, 1)
				);
			}
		}

		// Don't show the snap cursor if we're targeting a plane.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}

	return true;
}

bool UPlaneHostedObjTool::Deactivate()
{
	if (PendingObjMesh.IsValid())
	{
		PendingObjMesh->Destroy();
		PendingObjMesh.Reset();
	}

	if (Controller)
	{
		Controller->EMPlayerState->bShowSnappedCursor = bWasShowingSnapCursor;
	}

	return UMetaPlaneTool::Deactivate();
}

bool UPlaneHostedObjTool::HandleInputNumber(double n)
{
	return UMetaPlaneTool::HandleInputNumber(n);
}

bool UPlaneHostedObjTool::HandleSpacebar()
{
	if (!IsInUse())
	{
		return false;
	}
	if (PendingObjMesh.IsValid())
	{
		bInverted = !bInverted;
		ObjAssembly.InvertLayers();
	}
	return true;
}

bool UPlaneHostedObjTool::BeginUse()
{
	// TODO: require assemblies for stairs, once they can be crafted
	if (!Controller || (Assembly.Key.IsNone() && (ObjectType != EObjectType::OTStaircase)))
	{
		return false;
	}

	if (!PendingObjMesh.IsValid() && !PendingSegment.IsValid() && (LastValidTargetID != MOD_ID_NONE))
	{
		// Skip FMetaPlaneTool::BeginUse because we don't want to create any line segments in plane-application / "paint bucket" mode
		if (!UEditModelToolBase::BeginUse())
		{
			return false;
		}

		FModumateFunctionParameterSet result = Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kMakeMetaPlaneHostedObj)
			.Param(Parameters::kObjectType, EnumValueString(EObjectType, ObjectType))
			.Param(Parameters::kParent, LastValidTargetID)
			.Param(Parameters::kAssembly, Assembly.Key)
			.Param(Parameters::kOffset, GetDefaultJustificationValue())
			.Param(Parameters::kInverted, bInverted)
		);

		EndUse();
		return result.GetValue(Parameters::kSuccess);
	}
	else if (!bRequireHoverMetaPlane)
	{
		if (!UMetaPlaneTool::BeginUse())
		{
			return false;
		}

		if (GameMode.IsValid())
		{
			PendingObjMesh = GameMode->GetWorld()->SpawnActor<ADynamicMeshActor>(GameMode->DynamicMeshActorClass.Get());
			PendingObjMesh->SetActorHiddenInGame(true);
		}

		return true;
	}

	return false;
}

bool UPlaneHostedObjTool::EndUse()
{
	if (PendingObjMesh.IsValid())
	{
		PendingObjMesh->Destroy();
		PendingObjMesh = nullptr;
	}

	return UMetaPlaneTool::EndUse();
}

bool UPlaneHostedObjTool::AbortUse()
{
	if (PendingObjMesh.IsValid())
	{
		PendingObjMesh->Destroy();
		PendingObjMesh = nullptr;
	}

	return UMetaPlaneTool::AbortUse();
}

bool UPlaneHostedObjTool::EnterNextStage()
{
	return UMetaPlaneTool::EnterNextStage();
}

void UPlaneHostedObjTool::SetAssembly(const FShoppingItem &key)
{
	UMetaPlaneTool::SetAssembly(key);

	EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(ObjectType);
	const FModumateObjectAssembly *assembly = GameState.IsValid() ?
		GameState->GetAssemblyByKey_DEPRECATED(toolMode, key.Key) : nullptr;

	if (assembly != nullptr)
	{
		ObjAssembly = *assembly;

		// If we changed assembly, and we already wanted to be inverted,
		// then we need to make sure the layers are inverted now.
		if (bInverted)
		{
			ObjAssembly.InvertLayers();
		}
	}

	FrameUpdate();
}

bool UPlaneHostedObjTool::MakeObject(const FVector &Location, TArray<int32> &newObjIDs)
{
	Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kBeginUndoRedoMacro));

	int32 newObjID = MOD_ID_NONE;
	TArray<int32> newPlaneIDs;
	bool bSuccess = UMetaPlaneTool::MakeObject(Location, newPlaneIDs);

	for (int32 newPlaneID : newPlaneIDs)
	{
		FModumateObjectInstance *newPlaneObj = GameState->Document.GetObjectById(newPlaneID);

		if (newPlaneObj && (newPlaneObj->GetObjectType() == EObjectType::OTMetaPlane))
		{
			newObjID = Controller->ModumateCommand(
				FModumateCommand(Modumate::Commands::kMakeMetaPlaneHostedObj)
				.Param(Parameters::kObjectType, EnumValueString(EObjectType, ObjectType))
				.Param(Parameters::kParent, newPlaneID)
				.Param(Parameters::kAssembly, Assembly.Key)
				.Param(Parameters::kOffset, GetDefaultJustificationValue())
				.Param(Parameters::kInverted, bInverted)
			).GetValue(Parameters::kObjectID);

			newObjIDs.Add(newObjID);
		}
	}

	Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kEndUndoRedoMacro));

	return bSuccess;
}


UWallTool::UWallTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTWallSegment;
	SetAxisConstraint(EAxisConstraint::AxisZ);
}


UFloorTool::UFloorTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTFloorSegment;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}
