// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceGraph.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"

namespace Modumate
{
	FMOISurfaceGraphImpl::FMOISurfaceGraphImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, CachedFaceNormal(ForceInitToZero)
		, CachedFaceAxisX(ForceInitToZero)
		, CachedFaceAxisY(ForceInitToZero)
		, CachedFaceOrigin(ForceInitToZero)
	{
	}

	void FMOISurfaceGraphImpl::SetRotation(const FQuat &r)
	{
	}

	FQuat FMOISurfaceGraphImpl::GetRotation() const
	{
		return FRotationMatrix::MakeFromXY(CachedFaceAxisX, CachedFaceAxisY).ToQuat();
	}

	void FMOISurfaceGraphImpl::SetLocation(const FVector &p)
	{
	}

	FVector FMOISurfaceGraphImpl::GetLocation() const
	{
		return CachedFaceOrigin;
	}

	FVector FMOISurfaceGraphImpl::GetCorner(int32 index) const
	{
		return CachedFacePoints.IsValidIndex(index) ? CachedFacePoints[index] : GetLocation();
	}

	FVector FMOISurfaceGraphImpl::GetNormal() const
	{
		return CachedFaceNormal;
	}

	bool FMOISurfaceGraphImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
	{
		if (MOI == nullptr)
		{
			return false;
		}

		if (DirtyFlag == EObjectDirtyFlags::Structure)
		{
			PrevFacePoints = CachedFacePoints;

			const FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
			int32 faceIndex = UModumateObjectStatics::GetParentFaceIndex(MOI);
			if (!UModumateObjectStatics::GetGeometryFromFaceIndex(parentObj, faceIndex, CachedFacePoints, CachedFaceNormal, CachedFaceAxisX, CachedFaceAxisY))
			{
				return false;
			}

			CachedFaceOrigin = CachedFacePoints[0];

			// If the cached host face geometry has changed after it was created, then the surface graph may need to be updated or deleted to match the new host face
			if ((CachedFacePoints != PrevFacePoints) && (PrevFacePoints.Num() > 0))
			{
				if (OutSideEffectDeltas)
				{
					// TODO: try to generate deltas to update the surface graph, and only delete itself if that's not possible
					TArray<FModumateObjectInstance*> objectsToDelete = MOI->GetAllDescendents();
					objectsToDelete.Add(MOI);

					TArray<FMOIStateData> deletionStates;
					for (FModumateObjectInstance* descendent : objectsToDelete)
					{
						FMOIStateData& deletionState = deletionStates.AddDefaulted_GetRef();
						deletionState.StateType = EMOIDeltaType::Destroy;
						deletionState.ObjectID = descendent->ID;
					}

					auto deleteSurfaceDelta = MakeShareable(new FMOIDelta(deletionStates));
					OutSideEffectDeltas->Add(deleteSurfaceDelta);
				}
				else if (!ensure(parentObj->GetIsInPreviewMode() || (parentObj->GetParentObject() && parentObj->GetParentObject()->GetIsInPreviewMode())))
				{
					UE_LOG(LogTemp, Warning, TEXT("SurfaceGraph #%d's host #%d face #%d changed outside of preview modifications!"), MOI->ID, parentObj->ID, faceIndex);
				}
			}
		}

		return true;
	}
}
