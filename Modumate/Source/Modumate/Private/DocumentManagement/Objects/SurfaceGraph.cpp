// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceGraph.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"

namespace Modumate
{
	FMOISurfaceGraphImpl::FMOISurfaceGraphImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
	{
	}

	void FMOISurfaceGraphImpl::SetRotation(const FQuat &r)
	{
	}

	FQuat FMOISurfaceGraphImpl::GetRotation() const
	{
		return CachedFaceOrigin.GetRotation();
	}

	void FMOISurfaceGraphImpl::SetLocation(const FVector &p)
	{
	}

	FVector FMOISurfaceGraphImpl::GetLocation() const
	{
		return CachedFaceOrigin.GetLocation();
	}

	FVector FMOISurfaceGraphImpl::GetCorner(int32 index) const
	{
		return ensure(CachedFacePoints.IsValidIndex(index)) ? CachedFacePoints[index] : GetLocation();
	}

	FVector FMOISurfaceGraphImpl::GetNormal() const
	{
		return CachedFaceOrigin.GetUnitAxis(EAxis::Z);
	}

	bool FMOISurfaceGraphImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
	{
		if (DirtyFlag == EObjectDirtyFlags::Structure)
		{
			if (OutSideEffectDeltas == nullptr)
			{
				return true;
			}

			FModumateDocument* doc = MOI ? MOI->GetDocument() : nullptr;
			FGraph2D* surfaceGraph = doc ? doc->FindSurfaceGraph(MOI->ID) : nullptr;
			if (!ensure(doc && surfaceGraph) || !UpdateCachedGraphData())
			{
				return false;
			}

			// If the cached host face geometry has changed after it was created, then the surface graph may need to be updated or deleted to match the new host face
			int32 numPrevPoints = PrevFacePoints.Num();
			if ((CachedFacePoints != PrevFacePoints) && (numPrevPoints > 0))
			{
				TMap<int32, FVector2D> vertexMoves;

				if (numPrevPoints == CachedFacePoints.Num())
				{
					TArray<int32> boundingVertexIDs;
					surfaceGraph->GetOuterBoundsIDs(boundingVertexIDs);

					// For now, only attempt to generate new vertex movement positions for points that are part of the surface graph bounds,
					// and are included in the surface graph's mounting face point list
					for (int32 facePointIdx = 0; facePointIdx < numPrevPoints; ++facePointIdx)
					{
						FVector2D prevPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(PrevFacePoints[facePointIdx], PrevFaceOrigin);
						FGraph2DVertex* vertex = surfaceGraph->FindVertex(prevPos2D);
						if (ensure(vertex) && boundingVertexIDs.Contains(vertex->ID))
						{
							FVector2D newPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(CachedFacePoints[facePointIdx], CachedFaceOrigin);
							vertexMoves.Add(vertex->ID, newPos2D);
						}
					}
				}

				// Attempt to generate deltas with the projected positions
				int32 nextID = doc->GetNextAvailableID();
				TArray<FGraph2DDelta> moveDeltas;
				if ((vertexMoves.Num() > 0) && surfaceGraph->MoveVertices(moveDeltas, nextID, vertexMoves))
				{
					for (auto& delta : moveDeltas)
					{
						OutSideEffectDeltas->Add(MakeShareable(new FGraph2DDelta(delta)));
					}
				}
				// Otherwise, delete the surface graph if it cannot be preserved after the underlying geometry changes
				else
				{
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
			}
		}

		return true;
	}

	bool FMOISurfaceGraphImpl::UpdateCachedGraphData()
	{
		const FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
		if (!ensure(parentObj) || parentObj->IsDirty(EObjectDirtyFlags::Structure) || parentObj->IsDirty(EObjectDirtyFlags::Mitering))
		{
			return false;
		}

		// TODO: when we rely less on ControlPoints during preview mode, then we can more likely support cleaning surface graphs with preview graph data
		if (parentObj->GetIsInPreviewMode() || ((parentObj->GetParentObject() != nullptr) && parentObj->GetParentObject()->GetIsInPreviewMode()))
		{
			return false;
		}

		PrevFacePoints = CachedFacePoints;
		PrevFaceOrigin = CachedFaceOrigin;

		int32 faceIndex = UModumateObjectStatics::GetParentFaceIndex(MOI);
		return UModumateObjectStatics::GetGeometryFromFaceIndex(parentObj, faceIndex, CachedFacePoints, CachedFaceOrigin);
	}
}
