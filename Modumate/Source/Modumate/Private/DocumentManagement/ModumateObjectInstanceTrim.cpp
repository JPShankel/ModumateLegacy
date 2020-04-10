// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceTrim.h"

#include "DynamicMeshActor.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "ModumateSnappingView.h"

namespace Modumate
{
	FMOITrimImpl::FMOITrimImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, StartAlongEdge(0.0f)
		, EndAlongEdge(1.0f)
		, EdgeStartIndex(INDEX_NONE)
		, EdgeEndIndex(INDEX_NONE)
		, EdgeMountIndex(INDEX_NONE)
		, bUseLengthAsPercent(true)
		, MiterOptionStart(ETrimMiterOptions::None)
		, MiterOptionEnd(ETrimMiterOptions::None)
		, TrimStartPos(ForceInitToZero)
		, TrimEndPos(ForceInitToZero)
		, TrimNormal(ForceInitToZero)
		, TrimUp(ForceInitToZero)
		, TrimDir(ForceInitToZero)
		, UpperExtensions(ForceInitToZero)
		, OuterExtensions(ForceInitToZero)
	{
	}

	FMOITrimImpl::~FMOITrimImpl()
	{
	}

	void FMOITrimImpl::SetRotation(const FQuat &r)
	{
		MOI->MarkDirty(EObjectDirtyFlags::Structure);
	}

	FQuat FMOITrimImpl::GetRotation() const
	{
		const FModumateObjectInstance *parentMOI = MOI ? MOI->GetParentObject() : nullptr;
		if (parentMOI)
		{
			return parentMOI->GetObjectRotation();
		}

		return FQuat::Identity;
	}

	void FMOITrimImpl::SetLocation(const FVector &p)
	{
		MOI->MarkDirty(EObjectDirtyFlags::Structure);
	}

	FVector FMOITrimImpl::GetLocation() const
	{
		if (MOI && MOI->GetControlPoints().Num() >= 2)
		{
			return 0.5f * (MOI->GetControlPoint(0) + MOI->GetControlPoint(1));
		}

		return FVector::ZeroVector;
	}

	AActor *FMOITrimImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;

		AEditModelGameMode_CPP *gameMode = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		if (gameMode)
		{
			DynamicMeshActor = world->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get(), FTransform(rot, loc));

			if (MOI && DynamicMeshActor.IsValid() && DynamicMeshActor->Mesh)
			{
				ECollisionChannel collisionObjType = UModumateTypeStatics::CollisionTypeFromObjectType(MOI->GetObjectType());
				DynamicMeshActor->Mesh->SetCollisionObjectType(collisionObjType);
			}
		}

		return DynamicMeshActor.Get();
	}

	void FMOITrimImpl::SetupDynamicGeometry()
	{
		InternalUpdateGeometry(true, true);
	}

	void FMOITrimImpl::UpdateDynamicGeometry()
	{
		InternalUpdateGeometry(false, true);
	}

	void FMOITrimImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		const FSimplePolygon* profile = nullptr;
		if (UModumateObjectStatics::GetPolygonProfile(&MOI->GetAssembly(), profile))
		{
			FVector2D profileSize = profile->Extents.GetSize();

			// Trims are not centered on their line start/end positions; it is their origin.
			FVector upOffset = (profileSize.X * TrimUp);
			FVector normalOffset = (profileSize.Y * TrimNormal);

			FVector centroid = 0.5f * (TrimStartPos + TrimEndPos + upOffset + normalOffset);
			FVector boxExtents(FVector::Dist(TrimStartPos, TrimEndPos), profileSize.X, profileSize.Y);
			FQuat boxRot = FRotationMatrix::MakeFromYZ(TrimUp, TrimNormal).ToQuat();

			FModumateSnappingView::GetBoundingBoxPointsAndLines(centroid, boxRot, 0.5f * boxExtents, outPoints, outLines);
		}
	}

	void FMOITrimImpl::ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		FModumateObjectInstanceImplBase::ClearAdjustmentHandles(controller);

		// TODO: clear extension/retraction handles
	}

	void FMOITrimImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show)
	{
		FModumateObjectInstanceImplBase::ShowAdjustmentHandles(controller, show);

		// TODO: setup and show extension/retraction handles
	}

	FModumateWallMount FMOITrimImpl::GetWallMountForSelf(int32 originIndex) const
	{
		FModumateWallMount ret;
		ret.OriginIndex = originIndex;
		ret.RelativePosition = FVector::ZeroVector;
		ret.OrientationDelta = FQuat::Identity;

		if (MOI == nullptr)
		{
			return ret;
		}

		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (parent == nullptr)
		{
			return ret;
		}

		int32 edgeStartIndex = MOI->GetControlPointIndex(0);
		int32 edgeEndIndex = MOI->GetControlPointIndex(1);
		FVector edgeStartPos, edgeEndPos;
		if (!UModumateObjectStatics::GetTrimEdgePosition(parent, edgeStartIndex, edgeEndIndex, edgeStartPos, edgeEndPos))
		{
			return ret;
		}

		FVector edgeDelta = edgeEndPos - edgeStartPos;
		float edgeLength = edgeDelta.Size();

		ret.RelativePosition.X = edgeLength;
		ret.OriginalControlPoints = MOI->GetControlPoints();

		return ret;
	}

	void FMOITrimImpl::SetWallMountForSelf(const FModumateWallMount &wm)
	{
		if (MOI == nullptr)
		{
			return;
		}

		FModumateObjectInstance *parent = MOI->GetParentObject();
		if (parent == nullptr)
		{
			return;
		}

		int32 edgeStartIndex = MOI->GetControlPointIndex(0);
		int32 edgeEndIndex = MOI->GetControlPointIndex(1);
		FVector edgeStartPos, edgeEndPos;
		if (!UModumateObjectStatics::GetTrimEdgePosition(parent, edgeStartIndex, edgeEndIndex, edgeStartPos, edgeEndPos))
		{
			return;
		}

		FVector edgeDelta = edgeEndPos - edgeStartPos;
		float edgeLength = edgeDelta.Size();
		if (FMath::IsNearlyZero(edgeLength))
		{
			return;
		}
		FVector edgeDir = edgeDelta / edgeLength;

		// Restore original control points here, since they're being read/modified in order to adhere to wall edges.
		MOI->SetControlPoints(wm.OriginalControlPoints);

		float origStartAlongEdge = MOI->GetControlPoint(0).X;
		float origEndAlongEdge = MOI->GetControlPoint(1).X;
		float origEdgeLength = wm.RelativePosition.X;

		float edgeLengthDelta = edgeLength - origEdgeLength;

//		float &curStartAlongEdge = MOI->GetControlPoint(0).X;
//		float &curEndAlongEdge = MOI->GetControlPoint(1).X;

		FVector curStartAlongEdge = MOI->GetControlPoint(0);
		FVector curEndAlongEdge = MOI->GetControlPoint(1);

		curStartAlongEdge.X = FMath::Clamp(curStartAlongEdge.X, 0.0f, edgeLength);

		// If the start of the edge is being changed, then modify the start/end positions accordingly
		if (wm.OriginIndex == 0)
		{
			// If the original end point was at the end of the edge, then keep it there and extend the trim
			if (FMath::IsNearlyEqual(origEndAlongEdge, origEdgeLength))
			{
				curEndAlongEdge.X = FMath::Clamp(origEndAlongEdge + edgeLengthDelta, curStartAlongEdge.X, edgeLength);
			}
			else
			{
				curEndAlongEdge.X = FMath::Clamp(origEndAlongEdge, curStartAlongEdge.X, edgeLength);
			}
		}
		else if (wm.OriginIndex == 1)
		{
			// If the original start point was at the beginning of the edge, then keep it there and extend the trim
			if (!FMath::IsNearlyZero(origStartAlongEdge))
			{
				curStartAlongEdge.X = FMath::Clamp(origStartAlongEdge + edgeLengthDelta, 0.0f, edgeLength);
			}
			curEndAlongEdge.X = FMath::Clamp(origEndAlongEdge + edgeLengthDelta, curStartAlongEdge.X, edgeLength);
		}

		// Set the error state for this object, so that it can be deleted if it's rendered invalid by the current size
		AEditModelPlayerController_CPP *controller = World.IsValid() ? World->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		if (controller && controller->EMPlayerState)
		{
			static const FName zeroLengthErrorTag(TEXT("TrimZeroLength"));
			bool bZeroLengthError = FMath::IsNearlyEqual(curStartAlongEdge.X, curEndAlongEdge.X);
				controller->EMPlayerState->SetErrorForObject(MOI->ID, zeroLengthErrorTag, bZeroLengthError);
		}

		UpdateDynamicGeometry();
	}

	void FMOITrimImpl::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
	{
		// Updated cached values for this trim
		if (!ensure(DynamicMeshActor.IsValid() && MOI && (MOI->GetAssembly().Layers.Num() == 1) &&
			UModumateObjectStatics::GetTrimValuesFromControls(MOI->GetControlPoints(), MOI->GetControlPointIndices(),
				StartAlongEdge, EndAlongEdge, EdgeStartIndex, EdgeEndIndex, EdgeMountIndex,
				bUseLengthAsPercent, MiterOptionStart, MiterOptionEnd)))
		{
			return;
		}

		// This can be an expected error, if the object is still getting set up before it has a parent assigned.
		const FModumateObjectInstance *parentMOI = MOI ? MOI->GetParentObject() : nullptr;
		if (parentMOI == nullptr)
		{
			return;
		}

		// Get the trim geometry, using the saved mounting index in order to
		// provide a hint about which world normal and up vectors to use.
		if (ensure(UModumateObjectStatics::GetTrimGeometryOnEdge(parentMOI, &MOI->GetAssembly(), EdgeStartIndex, EdgeEndIndex,
			StartAlongEdge, EndAlongEdge, bUseLengthAsPercent, MiterOptionStart, MiterOptionEnd,
			TrimStartPos, TrimEndPos, TrimNormal, TrimUp, EdgeMountIndex,
			UpperExtensions, OuterExtensions, FVector::ZeroVector, EdgeMountIndex, true)))
		{
			TrimDir = (TrimEndPos - TrimStartPos).GetSafeNormal();

			FVector scaleVector;
			if (!MOI->GetAssembly().TryGetProperty(BIM::Parameters::Scale, scaleVector))
			{
				scaleVector = FVector::OneVector;
			}

			DynamicMeshActor->SetupExtrudedPolyGeometry(MOI->GetAssembly(), TrimStartPos, TrimEndPos,
				TrimNormal, TrimUp, UpperExtensions, OuterExtensions, scaleVector, bRecreate, bCreateCollision);
		}
	}
}
