// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPortalTools.h"

#include "DrawDebugHelpers.h"
#include "Graph/Graph3DFace.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/PortalFrameActor_CPP.h"
#include "Objects/Portal.h"

using namespace Modumate;

UPortalToolBase::UPortalToolBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
	CreateObjectMode = EToolCreateObjectMode::Stamp;
}

bool UPortalToolBase::Activate()
{
	CurTargetPlaneID = MOD_ID_NONE;
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	Active = true;
	return true;
}

bool UPortalToolBase::Deactivate()
{
	GameState->Document->ClearPreviewDeltas(GameState->GetWorld());

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

	CurTargetPlaneID = MOD_ID_NONE;

	if (snapCursor.Visible)
	{
		targetPlaneMOI = GameState->Document->ObjectFromActor(snapCursor.Actor);

		while (targetPlaneMOI && (targetPlaneMOI->GetObjectType() != EObjectType::OTMetaPlane))
		{
			targetPlaneMOI = targetPlaneMOI->GetParentObject();
		}
	}

	bool bSuccess = targetPlaneMOI && UModumateObjectStatics::GetRelativeTransformOnPlanarObj(
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
	const FGraph3DFace* curTargetFace = GameState->Document->GetVolumeGraph().FindFace(CurTargetPlaneID);

	if ((curTargetPlaneObj == nullptr) || (curTargetFace == nullptr))
	{
		return false;
	}

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::Draw:
		return false;
	case EToolCreateObjectMode::Apply:
	{
		newParentID = CurTargetPlaneID;

		if (curTargetPlaneObj->GetChildIDs().Num() > 0)
		{
			auto deleteChildrenDelta = MakeShared<FMOIDelta>();

			for (AModumateObjectInstance* curTargetPlaneChild : curTargetPlaneObj->GetChildObjects())
			{
				deleteChildrenDelta->AddCreateDestroyState(curTargetPlaneChild->GetStateData(), EMOIDeltaType::Destroy);
			}

			OutDeltas.Add(deleteChildrenDelta);
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

		TArray<int32> addedVertexIDs, addedEdgeIDs, addedFaceIDs;
		if (bValidContainedFace &&
			GameState->Document->MakeMetaObject(world, metaPlanePoints, {}, EObjectType::OTMetaPlane, MOD_ID_NONE,
				addedVertexIDs, addedEdgeIDs, addedFaceIDs, OutDeltas) &&
			(addedFaceIDs.Num() == 1))
		{
			newParentID = addedFaceIDs[0];
		}
	}
	break;
	default:
		return false;
	}

	if (newParentID != MOD_ID_NONE)
	{
		FMOIPortalData portalInstanceData;

		FMOIStateData objectStateData(GameState->Document->GetNextAvailableID(), UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode()), newParentID);
		objectStateData.AssemblyGUID = AssemblyGUID;
		objectStateData.CustomData.SaveStructData(portalInstanceData);

		auto addPortal = MakeShared<FMOIDelta>();
		addPortal->AddCreateDestroyState(objectStateData, EMOIDeltaType::Create);

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
	FVector nativeSize = assembly->GetRiggedAssemblyNativeSize();
	if (nativeSize.IsZero())
	{
		return false;
	}

	InstanceStampSize = nativeSize;
	return true;
}


UDoorTool::UDoorTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceStampSize = FVector(91.44f, 0.0f, 203.2f);
	InstanceBottomOffset = 0.0f;
}


UWindowTool::UWindowTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceStampSize = FVector(50.f, 0.0f, 100.f);
	InstanceBottomOffset = 91.44f;
}
