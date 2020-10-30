// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfaceGraph.h"

#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"

FMOISurfaceGraphImpl::FMOISurfaceGraphImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
{
}

FVector FMOISurfaceGraphImpl::GetLocation() const
{
	return CachedFaceOrigin.GetLocation();
}

FQuat FMOISurfaceGraphImpl::GetRotation() const
{
	return CachedFaceOrigin.GetRotation();
}

FVector FMOISurfaceGraphImpl::GetNormal() const
{
	return CachedFaceOrigin.GetUnitAxis(EAxis::Z);
}

FVector FMOISurfaceGraphImpl::GetCorner(int32 index) const
{
	return ensure(CachedFacePoints.IsValidIndex(index)) ? CachedFacePoints[index] : GetLocation();
}

int32 FMOISurfaceGraphImpl::GetNumCorners() const
{
	return CachedFacePoints.Num();
}

void FMOISurfaceGraphImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
}

bool FMOISurfaceGraphImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
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

		int32 numIDs = FaceIdxToVertexID.Num();
		// If the cached host face geometry has changed after it was created, then the surface graph may need to be updated or deleted to match the new host face
		if (numIDs > 0)
		{
			bool bFoundAllVertices = true;
			TMap<int32, FVector2D> vertexMoves;

			TArray<int32> boundingVertexIDs;
			surfaceGraph->GetOuterBoundsIDs(boundingVertexIDs);

			// For now, only attempt to generate new vertex movement positions for points that are part of the surface graph bounds,
			// and are included in the surface graph's mounting face point list
			int32 numCachedFacePoints = CachedFacePoints.Num();
			if (numCachedFacePoints != numIDs)
			{
				bFoundAllVertices = false;
			}
			else
			{
				for (int32 facePointIdx = 0; facePointIdx < numCachedFacePoints; ++facePointIdx)
				{
					Modumate::FGraph2DVertex* vertex = surfaceGraph->FindVertex(FaceIdxToVertexID[facePointIdx]);
					if (ensure(vertex) && boundingVertexIDs.Contains(vertex->ID))
					{
						FVector2D newPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(CachedFacePoints[facePointIdx], CachedFaceOrigin);
						if (!newPos2D.Equals(vertex->Position, PLANAR_DOT_EPSILON))
						{
							vertexMoves.Add(vertex->ID, newPos2D);
						}
					}
					else
					{
						bFoundAllVertices = false;
					}
				}
			}

			// Attempt to generate deltas with the projected positions
			int32 nextID = doc->GetNextAvailableID();
			TArray<FGraph2DDelta> moveDeltas;
			bool bValidGraph = bFoundAllVertices;
			if (bValidGraph && (vertexMoves.Num() > 0))
			{
				bValidGraph = surfaceGraph->MoveVertices(moveDeltas, nextID, vertexMoves);
			}

			if (bValidGraph)
			{
				for (auto& delta : moveDeltas)
				{
					OutSideEffectDeltas->Add(MakeShared<FGraph2DDelta>(delta));
				}
			}
			// Otherwise, delete the surface graph if it cannot be preserved after the underlying geometry changes
			else
			{
				TArray<FModumateObjectInstance*> objectsToDelete = MOI->GetAllDescendents();
				objectsToDelete.Add(MOI);

				auto deleteSurfaceDelta = MakeShared<FMOIDelta>();
				for (FModumateObjectInstance* descendent : objectsToDelete)
				{
					deleteSurfaceDelta->AddCreateDestroyState(descendent->GetStateData(), EMOIDeltaType::Destroy);
				}

				OutSideEffectDeltas->Add(deleteSurfaceDelta);
			}
		}
		else if (numIDs == 0)
		{
			TArray<int32> boundingVertexIDs;
			surfaceGraph->GetOuterBoundsIDs(boundingVertexIDs);
			for (int32 facePointIdx = 0; facePointIdx < CachedFacePoints.Num(); ++facePointIdx)
			{
				FVector2D prevPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(CachedFacePoints[facePointIdx], CachedFaceOrigin);
				Modumate::FGraph2DVertex* vertex = surfaceGraph->FindVertex(prevPos2D);
				if (ensure(vertex) && boundingVertexIDs.Contains(vertex->ID))
				{
					FaceIdxToVertexID.Add(facePointIdx, vertex->ID);
				}
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

	return UModumateObjectStatics::GetGeometryFromFaceIndex(parentObj, InstanceData.ParentFaceIndex, CachedFacePoints, CachedFaceOrigin);
}
