// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceGraph.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"

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

bool FMOISurfaceGraphImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas)
	{
		if (DirtyFlag == EObjectDirtyFlags::Structure)
		{
			// Only use the Prev* members for side-effect evaluation, in order to know which two reference frames the surface graph elements are moving between
			if (OutSideEffectDeltas != nullptr)
			{
				PrevFacePoints = CachedFacePoints;
				PrevFaceOrigin = CachedFaceOrigin;
			}

			FModumateDocument* doc = MOI ? MOI->GetDocument() : nullptr;
			if (!ensure(doc))
			{
				return false;
			}

			auto surfaceGraph = doc->FindSurfaceGraph(MOI->ID);
			if (!ensure(surfaceGraph.IsValid()) || !UpdateCachedGraphData())
			{
				return false;
			}

			// If we aren't evaluating side effects, then updating cached graph data (and subsequently dirtying children surface graph elements) is enough
			if (OutSideEffectDeltas == nullptr)
			{
				return true;
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
					Modumate::FGraph2DVertex* vertex = surfaceGraph->FindVertex(prevPos2D);
						if (ensure(vertex) && boundingVertexIDs.Contains(vertex->ID))
						{
							FVector2D newPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(CachedFacePoints[facePointIdx], CachedFaceOrigin);
							vertexMoves.Add(vertex->ID, newPos2D);
						}
					}
				}

				// Attempt to generate deltas with the projected positions
				int32 nextID = doc->GetNextAvailableID();
			TArray<Modumate::FGraph2DDelta> moveDeltas;
				if ((vertexMoves.Num() > 0) && surfaceGraph->MoveVertices(moveDeltas, nextID, vertexMoves))
				{
					for (auto& delta : moveDeltas)
					{
					OutSideEffectDeltas->Add(MakeShareable(new Modumate::FGraph2DDelta(delta)));
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

		int32 faceIndex = UModumateObjectStatics::GetParentFaceIndex(MOI);
		return UModumateObjectStatics::GetGeometryFromFaceIndex(parentObj, faceIndex, CachedFacePoints, CachedFaceOrigin);
	}
