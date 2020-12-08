// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfaceGraph.h"

#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
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
	int32 numVerts = GraphVertexToBoundVertex.Num();

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

		auto hostObj = doc->GetObjectById(MOI->GetParentID());
		if (hostObj == nullptr)
		{
			return false;
		}

		auto faceObj = doc->GetObjectById(hostObj->GetParentID());
		if (faceObj == nullptr)
		{
			return false;
		}

		auto& graph = doc->GetVolumeGraph();
		auto face = graph.FindFace(faceObj->ID);
		if (face == nullptr)
		{
			return false;
		}

		// If we aren't evaluating side effects, then updating cached graph data (and subsequently dirtying children surface graph elements) is enough
		if (OutSideEffectDeltas == nullptr)
		{
			CheckGraphLink();

			if (FVector::Parallel(face->CachedPlane, MOI->GetNormal()))
			{
				GraphVertexToBoundVertex.Reset();
				GraphFaceToInnerBound.Reset();

				TArray<int32> addIDs;
				for (int32 containedFaceID : face->ContainedFaceIDs)
				{
					if (!GraphFaceToInnerBound.Contains(containedFaceID))
					{
						addIDs.Add(containedFaceID);
					}
				}

				TMap<int32, TArray<FVector2D>> graphPolygonsToAdd;
				TMap<int32, TArray<int32>> graphFaceToVertices;
				if (CalculateFaces(addIDs, graphPolygonsToAdd, graphFaceToVertices))
				{
					for (auto& kvp : graphPolygonsToAdd)
					{
						const auto* containedFace = graph.FindFace(kvp.Key);

						for (int32 idx = 0; idx < kvp.Value.Num(); idx++)
						{
							FVector2D pos = kvp.Value[idx];
							int32 vertexID = containedFace->VertexIDs[idx];

							if (auto surfaceVertex = surfaceGraph->FindVertex(pos))
							{
								GraphVertexToBoundVertex.Add(vertexID, surfaceVertex->ID);
							}
						}
					}

					TArray<int32> edges;
					surfaceGraph->GetEdges().GenerateKeyArray(edges);

					if (!surfaceGraph->FindVerticesAndPolygons(graphPolygonsToAdd, GraphFaceToInnerBound, GraphVertexToBoundVertex, edges))
					{
						return true;
					}
				}
			}

			return true;
		}

		int32 numIDs = FaceIdxToVertexID.Num();
		if (numIDs == 0)
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
		numIDs = FaceIdxToVertexID.Num();
		// If the cached host face geometry has changed after it was created, then the surface graph may need to be updated or deleted to match the new host face
		if (ensureAlways(numIDs > 0))
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
				if (!surfaceGraph->IsOuterBoundsDirty() && bLinked)
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
				else
				{
					CheckGraphLink();
				}
			}

			// Attempt to update the surface graph with the correct bounds
			int32 nextID = doc->ReserveNextIDs(MOI->ID);
			
			// movement updates
			for (auto& kvp : GraphVertexToBoundVertex)
			{
				auto graphVertex = graph.FindVertex(kvp.Key);
				auto surfaceVertex = surfaceGraph->FindVertex(kvp.Value);

				if (!graphVertex || !surfaceVertex)
				{
					continue;
				}

				FVector2D newPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(graphVertex->Position, CachedFaceOrigin);
				if (!newPos2D.Equals(surfaceVertex->Position, PLANAR_DOT_EPSILON))
				{
					vertexMoves.Add(surfaceVertex->ID, newPos2D);
				}
			}

			if (FVector::Parallel(face->CachedPlane, MOI->GetNormal()))
			{
				// removes
				TArray<FGraph2DDelta> deleteDeltas;
				TArray<int32> deleteIDs;
				for (auto& kvp : GraphFaceToInnerBound)
				{
					// face in the map is no longer contained
					if (kvp.Key != MOD_ID_NONE && !face->ContainedFaceIDs.Contains(kvp.Key))
					{
						deleteIDs.Add(kvp.Value);
					}
				}
				for (int32 id : deleteIDs)
				{
					GraphFaceToInnerBound.Remove(id);
				}

				surfaceGraph->DeleteObjects(deleteDeltas, nextID, deleteIDs);
				TSet<int32> deletedObjects;
				for (auto& delta : deleteDeltas)
				{
					delta.AggregateDeletedObjects(deletedObjects);
				}

				TSet<int32> removedVertices;
				removedVertices.Reset();
				for (auto& kvp : GraphVertexToBoundVertex)
				{
					if (deletedObjects.Contains(kvp.Value))
					{
						removedVertices.Add(kvp.Key);
					}
				}
				for (int32 id : removedVertices)
				{
					GraphVertexToBoundVertex.Remove(id);
				}

				for (auto& delta : deleteDeltas)
				{
					OutSideEffectDeltas->Add(MakeShared<FGraph2DDelta>(delta));
				}

				// adds
				TArray<int32> addIDs;
				for (int32 containedFaceID : face->ContainedFaceIDs)
				{
					if (!GraphFaceToInnerBound.Contains(containedFaceID))
					{
						addIDs.Add(containedFaceID);
					}
				}

				TMap<int32, TArray<FVector2D>> graphPolygonsToAdd;
				TMap<int32, TArray<int32>> graphFaceToVertices;
				if (CalculateFaces(addIDs, graphPolygonsToAdd, graphFaceToVertices))
				{
					int32 rootPolyID;
					TArray<FGraph2DDelta> boundsDeltas;
					if (!surfaceGraph->PopulateFromPolygons(boundsDeltas, nextID, graphPolygonsToAdd, graphFaceToVertices, GraphFaceToInnerBound, GraphVertexToBoundVertex, true, rootPolyID))
					{
						doc->SetNextID(doc->GetNextAvailableID(), MOI->ID);
						return true;
					}

					for (auto& delta : boundsDeltas)
					{
						OutSideEffectDeltas->Add(MakeShared<FGraph2DDelta>(delta));
					}
				}
			}

			// surface graph can only have inner bounds (that are mirrored by the volume graph)
			// if its normal is parallel to the plane it is hosted on

			// Attempt to generate deltas with the projected positions
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

			doc->SetNextID(nextID, MOI->ID);
		}
	}

	numVerts = GraphVertexToBoundVertex.Num();

	return true;
}

bool FMOISurfaceGraphImpl::CheckGraphLink()
{
	FModumateDocument* doc = MOI ? MOI->GetDocument() : nullptr;
	auto surfaceGraph = doc ? doc->FindSurfaceGraph(MOI->ID) : nullptr;
	if (!ensure(doc && surfaceGraph))
	{
		return false;
	}

	surfaceGraph->ClearOuterBoundsDirty();

	int32 numIDs = FaceIdxToVertexID.Num();
	if (numIDs == 0)
	{
		return true;
	}

	int32 numCachedFacePoints = CachedFacePoints.Num();
	if (numCachedFacePoints != numIDs)
	{
		return false;
	}

	TArray<int32> boundingVertexIDs;
	surfaceGraph->GetOuterBoundsIDs(boundingVertexIDs);

	for (int32 facePointIdx = 0; facePointIdx < numCachedFacePoints; ++facePointIdx)
	{
		Modumate::FGraph2DVertex* vertex = surfaceGraph->FindVertex(FaceIdxToVertexID[facePointIdx]);
		if (ensure(vertex) && boundingVertexIDs.Contains(vertex->ID))
		{
			FVector2D newPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(CachedFacePoints[facePointIdx], CachedFaceOrigin);
			if (!newPos2D.Equals(vertex->Position, PLANAR_DOT_EPSILON))
			{
				bLinked = false;
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	bLinked = true;
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

bool FMOISurfaceGraphImpl::CalculateFaces(const TArray<int32>& AddIDs, TMap<int32, TArray<FVector2D>>& OutPolygonsToAdd, TMap<int32, TArray<int32>>& OutFaceToVertices)
{
	if (AddIDs.Num() == 0)
	{
		return false;
	}

	FModumateDocument* doc = MOI ? MOI->GetDocument() : nullptr;
	if (!ensure(doc))
	{
		return false;
	}
	auto& graph = doc->GetVolumeGraph();

	int32 hitFaceIndex = UModumateObjectStatics::GetParentFaceIndex(MOI);
	TArray<FVector> cornerPositions;
	FVector origin, normal, axisX, axisY;

	auto hostmoi = doc->GetObjectById(MOI->GetParentID());
	if (hostmoi && UModumateObjectStatics::GetGeometryFromFaceIndex(hostmoi, hitFaceIndex,
		cornerPositions, normal, axisX, axisY))
	{
		origin = cornerPositions[0];

		for (int32 id : AddIDs)
		{
			const auto* containedFace = graph.FindFace(id);
			TArray<FVector2D> holePolygon;
			Algo::Transform(containedFace->CachedPositions, holePolygon, [this, axisX, axisY, origin](const FVector& WorldPoint) {
				return UModumateGeometryStatics::ProjectPoint2D(WorldPoint, axisX, axisY, origin);
				});
			OutPolygonsToAdd.Add(id, holePolygon);
			OutFaceToVertices.Add(id, containedFace->VertexIDs);
		}
	}

	return true;
}
