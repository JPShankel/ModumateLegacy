// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Objects/PlaneHostedObj.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"

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

bool UPlaneHostedObjTool::IsTargetFacingDown()
{
	if (LastValidTargetID != MOD_ID_NONE)
	{
		FModumateObjectInstance *parentMOI = GameState->Document.GetObjectById(LastValidTargetID);
		return (parentMOI && (parentMOI->GetNormal().Z < 0.0f));
	}
	else if (bPendingPlaneValid && PendingPlaneGeom.IsNormalized())
	{
		return (PendingPlaneGeom.Z < 0.0f);
	}

	return false;
}

float UPlaneHostedObjTool::GetDefaultJustificationValue()
{
	if (GameState == nullptr)
	{
		return 0.5f;
	}

	// For walls, use the default justification for vertically-oriented objects
	if (ObjectType == EObjectType::OTWallSegment)
	{
		return GameState->Document.GetDefaultJustificationZ();
	}

	// Otherwise, start with the default justification for horizontally-oriented objects
	float defaultJustificationXY = GameState->Document.GetDefaultJustificationXY();

	// Now flip the justification if the target plane is not facing downwards
	if (!IsTargetFacingDown())
	{
		defaultJustificationXY = (1.0f - defaultJustificationXY);
	}

	return defaultJustificationXY;
}

bool UPlaneHostedObjTool::GetAppliedInversionValue()
{
	// When applying walls to meta planes, keep the original inversion value
	if (ObjectType == EObjectType::OTWallSegment)
	{
		return bInverted;
	}

	// Otherwise, use the same criteria as justification values, where if the target is not facing down,
	// flip the inversion value so that it matches the intent for object types that lie flat by default.
	bool bInversionValue = bInverted;

	if (!IsTargetFacingDown())
	{
		bInversionValue = !bInversionValue;
	}

	return bInversionValue;
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

	if (PendingObjMesh.IsValid() && PendingSegmentID != MOD_ID_NONE)
	{
		auto pendingSegment = DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();

		if (bPendingPlaneValid && (PendingPlanePoints.Num() >= 3))
		{
			TArray<FPolyHole3D> holes;
			bool bRecreatingGeometry = (PendingObjMesh->LayerGeometries.Num() == 0);
			PendingObjMesh->CreateBasicLayerDefs(PendingPlanePoints, FVector::ZeroVector, holes, ObjAssembly, GetDefaultJustificationValue());
			PendingObjMesh->UpdatePlaneHostedMesh(bRecreatingGeometry, false, false, PendingPlanePoints[0]);
			PendingObjMesh->SetActorHiddenInGame(false);
		}
		else
		{
			PendingObjMesh->SetActorHiddenInGame(true);
		}

		if (pendingSegment)
		{
			pendingSegment->SetActorHiddenInGame(!bPendingSegmentValid || bPendingPlaneValid);
		}

		// Always hide the pending plane mesh inherited from the MetaPlaneTool
		if (PendingPlane.IsValid())
		{
			PendingPlane->SetActorHiddenInGame(true);
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
			int32 numCorners = hitMOI->GetNumCorners();
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

bool UPlaneHostedObjTool::HandleInvert()
{
	if (!IsInUse())
	{
		return false;
	}
	if (PendingObjMesh.IsValid())
	{
		bInverted = !bInverted;
		ObjAssembly.ReverseLayers();
	}
	return true;
}

bool UPlaneHostedObjTool::BeginUse()
{
	// TODO: require assemblies for stairs, once they can be crafted
	if (!Controller || (AssemblyKey.IsNone() && (ObjectType != EObjectType::OTStaircase)))
	{
		return false;
	}

	if (!PendingObjMesh.IsValid() && (LastValidTargetID != MOD_ID_NONE))
	{
		// Skip FMetaPlaneTool::BeginUse because we don't want to create any line segments in plane-application / "paint bucket" mode
		if (!UEditModelToolBase::BeginUse())
		{
			return false;
		}

		FModumateObjectInstance* parentMOI = GameState->Document.GetObjectById(LastValidTargetID);

		if (ensureAlways(parentMOI != nullptr))
		{
			auto delta = MakeShared<FMOIDelta>();
			FModumateObjectInstance* existingLayeredObj = nullptr;
			for (auto child : parentMOI->GetChildObjects())
			{
				if ((child->GetLayeredInterface() != nullptr) && ensureAlways(existingLayeredObj == nullptr))
				{
					existingLayeredObj = child;

					if (child->GetObjectType() == ObjectType)
					{
						FMOIStateData& newState = delta->AddMutationState(child);
						newState.AssemblyKey = AssemblyKey;

						GameState->Document.ApplyDeltas({ delta }, GetWorld());

						EndUse();
						return false;
					}
					else
					{
						delta->AddCreateDestroyState(child->GetStateData(), EMOIDeltaType::Destroy);
					}
				}
			}

			FMOIPlaneHostedObjData newMOICustomData;
			newMOICustomData.bLayersInverted = GetAppliedInversionValue();
			newMOICustomData.Justification = GetDefaultJustificationValue();

			FMOIStateData newMOIData;
			newMOIData.ID = GameState->Document.GetNextAvailableID();
			newMOIData.ObjectType = ObjectType;
			newMOIData.ParentID = LastValidTargetID;
			newMOIData.AssemblyKey = AssemblyKey;
			newMOIData.CustomData.SaveStructData(newMOICustomData);
			delta->AddCreateDestroyState(newMOIData, EMOIDeltaType::Create);

			GameState->Document.ApplyDeltas({ delta }, GetWorld());
			EndUse();
		}
		else
		{
			EndUse();
			return false;
		}
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

void UPlaneHostedObjTool::SetAssemblyKey(const FBIMKey& InAssemblyKey)
{
	UMetaPlaneTool::SetAssemblyKey(InAssemblyKey);

	EToolMode toolMode = UModumateTypeStatics::ToolModeFromObjectType(ObjectType);
	const FBIMAssemblySpec *assembly = GameState.IsValid() ?
		GameState->Document.PresetManager.GetAssemblyByKey(toolMode, InAssemblyKey) : nullptr;

	if (assembly != nullptr)
	{
		ObjAssembly = *assembly;

		// If we changed assembly, and we already wanted to be inverted,
		// then we need to make sure the layers are inverted now.
		if (bInverted)
		{
			ObjAssembly.ReverseLayers();
		}
	}

	FrameUpdate();
}

bool UPlaneHostedObjTool::MakeObject(const FVector &Location, TArray<int32> &newObjIDs)
{
	Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kBeginUndoRedoMacro));

	TArray<int32> newGraphObjIDs;
	bool bSuccess = UMetaPlaneTool::MakeObject(Location, newGraphObjIDs);

	if (bSuccess)
	{
		auto delta = MakeShared<FMOIDelta>();
		int32 nextID = GameState->Document.GetNextAvailableID();

		FMOIPlaneHostedObjData newMOICustomData;
		newMOICustomData.bLayersInverted = bInverted;
		newMOICustomData.Justification = GetDefaultJustificationValue();

		for (int32 newGraphObjID : newGraphObjIDs)
		{
			FModumateObjectInstance *newGraphObj = GameState->Document.GetObjectById(newGraphObjID);

			if (newGraphObj && (newGraphObj->GetObjectType() == EObjectType::OTMetaPlane))
			{
				FMOIStateData newMOIData;
				newMOIData.ID = nextID++;
				newMOIData.ObjectType = ObjectType;
				newMOIData.ParentID = newGraphObjID;
				newMOIData.AssemblyKey = AssemblyKey;
				newMOIData.CustomData.SaveStructData(newMOICustomData);
				delta->AddCreateDestroyState(newMOIData, EMOIDeltaType::Create);

				newObjIDs.Add(newMOIData.ID);
			}
		}

		GameState->Document.ApplyDeltas({ delta }, GetWorld());
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

URoofFaceTool::URoofFaceTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTRoofFace;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}

UCeilingTool::UCeilingTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTCeiling;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}

UCountertopTool::UCountertopTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTCountertop;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}

UPanelTool::UPanelTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTSystemPanel;
	SetAxisConstraint(EAxisConstraint::AxisZ);
}
