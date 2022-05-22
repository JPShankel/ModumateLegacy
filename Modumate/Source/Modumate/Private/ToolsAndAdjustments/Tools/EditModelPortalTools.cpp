// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPortalTools.h"

#include "DrawDebugHelpers.h"
#include "Graph/Graph3DFace.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Database/ModumateObjectDatabase.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "Objects/MetaPlaneSpan.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/PortalFrameActor.h"
#include "Objects/Portal.h"



UPortalToolBase::UPortalToolBase()
	: Super()
	, CurTargetPlaneID(MOD_ID_NONE)
	, bUseBottomOffset(true)
	, WorldPos(FVector::ZeroVector)
	, RelativePos(FVector2D::ZeroVector)
	, WorldRot(FQuat::Identity)
	, RelativeRot(FQuat::Identity)
	, Active(false)
	, Inverted(false)
	, bValidPortalConfig(false)
	, InstanceStampSize(FVector::ZeroVector)
	, InstanceBottomOffset(0.0f)
{
	CreateObjectMode = EToolCreateObjectMode::Apply;
}

bool UPortalToolBase::Activate()
{
	// Set default settings for NewMOIStateData
	FMOIPortalData portalInstanceData(FMOIPortalData::CurrentVersion);
	NewMOIStateData.ObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode());
	NewMOIStateData.CustomData.SaveStructData(portalInstanceData);

	CurTargetPlaneID = MOD_ID_NONE;
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	bWasShowingSnapCursor = Controller->EMPlayerState->bShowSnappedCursor;
	Active = true;
	return true;
}

bool UPortalToolBase::Deactivate()
{
	GameState->Document->ClearPreviewDeltas(GameState->GetWorld());
	Controller->EMPlayerState->bShowSnappedCursor = bWasShowingSnapCursor;

	Active = false;
	return true;
}

bool UPortalToolBase::FrameUpdate()
{
	int32 lastTargetPlaneID = CurTargetPlaneID;
	FVector lastWorldPos = WorldPos;
	FQuat lastWorldRot = WorldRot;

	UWorld* world = Controller->GetWorld();
	AModumateObjectInstance *targetPlaneMOI = nullptr;
	const auto &snapCursor = Controller->EMPlayerState->SnappedCursor;
	FVector hitLoc = snapCursor.WorldPosition;

	Controller->EMPlayerState->bShowSnappedCursor = CreateObjectMode != EToolCreateObjectMode::Apply;

	CurTargetPlaneID = MOD_ID_NONE;

	if (snapCursor.Visible)
	{
		targetPlaneMOI = GameState->Document->ObjectFromActor(snapCursor.Actor);

		while (targetPlaneMOI && (targetPlaneMOI->GetObjectType() != EObjectType::OTMetaPlane))
		{
			targetPlaneMOI = targetPlaneMOI->GetParentObject();
		}
	}

	bool bSuccess = targetPlaneMOI && (GameState->Document->FindGraph3DByObjID(targetPlaneMOI->ID) == GameState->Document->GetActiveVolumeGraphID());
	bSuccess = bSuccess && UModumateObjectStatics::GetRelativeTransformOnPlanarObj(
		targetPlaneMOI, hitLoc, GetInstanceBottomOffset(), bUseBottomOffset, RelativePos, RelativeRot);

	bSuccess = bSuccess && UModumateObjectStatics::GetWorldTransformOnPlanarObj(
		targetPlaneMOI, RelativePos, RelativeRot, WorldPos, WorldRot);

#if UE_BUILD_DEBUG
	if (bSuccess)
	{
		DrawDebugCoordinateSystem(world, WorldPos, WorldRot.Rotator(), 20.0f, false, -1.f, 255, 2.0f);
	}
#endif

	CurTargetPlaneID = (bSuccess && targetPlaneMOI) ? targetPlaneMOI->ID : MOD_ID_NONE;

	// TODO: it would be great to optimize and only re-apply preview deltas if the results would be different than the last frame,
	// but that's only possible if the application of preview deltas doesn't affect cursor results.
	if (bSuccess)
	{
		GameState->Document->StartPreviewing();
		if (GetPortalCreationDeltas(Deltas))
		{
			GameState->Document->ApplyPreviewDeltas(Deltas, world);
		}
	}

	return true;
}

bool UPortalToolBase::EnterNextStage()
{
	return true;
}

bool UPortalToolBase::EndUse()
{
	return true;
}

bool UPortalToolBase::ScrollToolOption(int32 dir)
{
	return true;
}

bool UPortalToolBase::HandleInputNumber(double n)
{
	return true;
}

bool UPortalToolBase::AbortUse()
{
	return true;
}

bool UPortalToolBase::HandleInvert()
{
	if (!IsInUse())
	{
		return false;
	}
	Inverted = !Inverted;
	return true;
}

bool UPortalToolBase::HandleFlip(EAxis::Type FlipAxis)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	return newMOI && newMOI->GetFlippedState(FlipAxis, NewMOIStateData);
}

bool UPortalToolBase::HandleControlKey(bool pressed)
{
	return true;
}

bool UPortalToolBase::BeginUse()
{
	UWorld* world = Controller->GetWorld();
	GameState->Document->ClearPreviewDeltas(world);

	if (GetPortalCreationDeltas(Deltas))
	{
		GameState->Document->ApplyDeltas(Deltas, world);
	}

	return true;
}

bool UPortalToolBase::HandleOffset(const FVector2D& ViewSpaceDirection)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	FQuat cameraRotation = Controller->PlayerCameraManager->GetCameraRotation().Quaternion();
	FVector worldSpaceDirection =
		(ViewSpaceDirection.X * cameraRotation.GetRightVector()) +
		(ViewSpaceDirection.Y * cameraRotation.GetUpVector());

	// Offset portal moi
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	return newMOI && newMOI->GetOffsetState(worldSpaceDirection, NewMOIStateData);
}

void UPortalToolBase::SetInstanceWidth(const float InWidth)
{
	InstanceStampSize.X = InWidth;
}

void UPortalToolBase::SetInstanceHeight(const float InHeight)
{
	InstanceStampSize.Z = InHeight;
}

void UPortalToolBase::SetInstanceBottomOffset(const float InBottomOffset)
{
	InstanceBottomOffset = InBottomOffset;
}

float UPortalToolBase::GetInstanceWidth() const
{
	return InstanceStampSize.X;
}

float UPortalToolBase::GetInstanceHeight() const
{
	return InstanceStampSize.Z;
}

float UPortalToolBase::GetInstanceBottomOffset() const
{
	return InstanceBottomOffset;
}

void UPortalToolBase::OnAssemblyChanged()
{
	Super::OnAssemblyChanged();

	CalculateNativeSize();
}

bool UPortalToolBase::GetPortalCreationDeltas(TArray<FDeltaPtr>& OutDeltas)
{
	OutDeltas.Reset();

	UWorld* world = Controller->GetWorld();
	int32 newParentID = MOD_ID_NONE;
	AModumateObjectInstance* curTargetPlaneObj = GameState->Document->GetObjectById(CurTargetPlaneID);
	const FGraph3DFace* curTargetFace = curTargetPlaneObj ? GameState->Document->FindVolumeGraph(CurTargetPlaneID)->FindFace(CurTargetPlaneID)
		: nullptr;

	if ((curTargetPlaneObj == nullptr) || (curTargetFace == nullptr))
	{
		return false;
	}

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::Draw:
		return false;
	case EToolCreateObjectMode::Add:
	{
		newParentID = CurTargetPlaneID;
		break;
	}
	case EToolCreateObjectMode::Apply:
	{
		newParentID = CurTargetPlaneID;

		TArray<int32> spans;
		UModumateObjectStatics::GetSpansForFaceObject(GameState->Document, curTargetPlaneObj, spans);

		if (curTargetPlaneObj->GetChildIDs().Num() > 0 || spans.Num() > 0)
		{
			auto deleteChildrenDelta = MakeShared<FMOIDelta>();

			for (AModumateObjectInstance* curTargetPlaneChild : curTargetPlaneObj->GetChildObjects())
			{
				deleteChildrenDelta->AddCreateDestroyState(curTargetPlaneChild->GetStateData(), EMOIDeltaType::Destroy);
			}

			OutDeltas.Add(deleteChildrenDelta);

			for (auto spanID : spans)
			{
				// TODO: face hosted objects to join span instead of replace, requires hole support in span geometry
				FModumateObjectDeltaStatics::GetDeltasForFaceSpanAddRemove(GameState->Document, spanID, {}, { curTargetPlaneObj->ID }, OutDeltas);
			}
		}
	}
	break;
	case EToolCreateObjectMode::Stamp:
	{
		FTransform portalTransform(WorldRot, WorldPos, FVector::OneVector);

		TArray<FVector> metaPlanePoints = {
			{0, 0, 0}, { 0, 0, InstanceStampSize.Z },
			{InstanceStampSize.X, 0, InstanceStampSize.Z}, { InstanceStampSize.X, 0, 0 }
		};

		for (FVector& metaPlanePoint : metaPlanePoints)
		{
			metaPlanePoint = portalTransform.TransformPosition(metaPlanePoint);
		}

		// Require that stamped faces are either fully or partially contained by the target face
		bool bValidContainedFace = true;
		for (const FVector& metaPlanePoint : metaPlanePoints)
		{
			bool bPointOverlaps;
			if (!curTargetFace->ContainsPosition(metaPlanePoint, bPointOverlaps) && !bPointOverlaps)
			{
				bValidContainedFace = false;
			}

			FPointInPolyResult holeTestResult;
			FVector2D projectedPoint = curTargetFace->ProjectPosition2D(metaPlanePoint);
			for (auto& hole : curTargetFace->Cached2DHoles)
			{
				if (!UModumateGeometryStatics::TestPointInPolygon(projectedPoint, hole.Points, holeTestResult) || holeTestResult.bInside)
				{
					bValidContainedFace = false;
					break;
				}
			}

			if (!bValidContainedFace)
			{
				break;
			}
		}

		TArray<int32> addedFaceIDs;
		TArray<FGraph3DDelta> graphDeltas;
		if (bValidContainedFace &&
			GameState->Document->MakeMetaObject(world, metaPlanePoints, addedFaceIDs, OutDeltas, graphDeltas) &&
			(addedFaceIDs.Num() == 1))
		{
			// Assign new face from UModumateDocument::MakeMetaObject() as new parent face for portal
			newParentID = addedFaceIDs[0];

			// Use graph deltas to get face info for span
			TArray<int32> deltaFaceAdditions;
			TArray<int32> deltaFaceDeletions;
			for (auto curGraphDelta : graphDeltas)
			{
				for (auto& kvp : curGraphDelta.FaceAdditions)
				{
					// Span does not include portal face
					if (kvp.Key != newParentID)
					{
						deltaFaceAdditions.Add(kvp.Key);
					}

				}
				for (auto& kvp : curGraphDelta.FaceDeletions)
				{
					deltaFaceDeletions.Add(kvp.Key);
				}
			}

			TArray<int32> spans;
			UModumateObjectStatics::GetSpansForFaceObject(GameState->Document, curTargetPlaneObj, spans);
			for (auto spanID : spans)
			{
				// TODO: face hosted objects to join span instead of replace, requires hole support in span geometry
				FModumateObjectDeltaStatics::GetDeltasForFaceSpanAddRemove(GameState->Document, spanID, deltaFaceAdditions, deltaFaceDeletions, OutDeltas);
			}
		}
	}
	break;
	default:
		return false;
	}

	if (newParentID != MOD_ID_NONE)
	{
		int32 nextPortalObjID = GameState->Document->GetNextAvailableID();
		NewObjectIDs = { nextPortalObjID };
		NewMOIStateData.ID = nextPortalObjID;
		NewMOIStateData.ParentID = newParentID;
		NewMOIStateData.AssemblyGUID = AssemblyGUID;

		auto addPortal = MakeShared<FMOIDelta>();
		addPortal->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);

		OutDeltas.Add(addPortal);
		return true;
	}

	return OutDeltas.Num() > 0;
}

bool UPortalToolBase::CalculateNativeSize()
{
	const FBIMAssemblySpec* assembly = GameState->Document->GetPresetCollection().GetAssemblyByGUID(GetToolMode(), AssemblyGUID);
	if (assembly == nullptr)
	{
		return false;
	}

	// Calculate the native size of the mesh
	FVector nativeSize = assembly->GetCompoundAssemblyNativeSize();
	if (nativeSize.IsZero())
	{
		return false;
	}

	InstanceStampSize = nativeSize;
	return true;
}


UDoorTool::UDoorTool()
	: Super()
{
	InstanceStampSize = FVector(91.44f, 0.0f, 203.2f);
	InstanceBottomOffset = 0.0f;
}


UWindowTool::UWindowTool()
	: Super()
{
	InstanceStampSize = FVector(50.f, 0.0f, 100.f);
	InstanceBottomOffset = 91.44f;
}
