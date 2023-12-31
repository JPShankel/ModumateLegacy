// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfaceGraph.h"

#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/MetaPlaneSpan.h"
#include "DocumentManagement/ModumateDocument.h"

AMOISurfaceGraph::AMOISurfaceGraph()
	: AModumateObjectInstance()
{
}

FVector AMOISurfaceGraph::GetLocation() const
{
	return CachedFaceOrigin.GetLocation();
}

FQuat AMOISurfaceGraph::GetRotation() const
{
	return CachedFaceOrigin.GetRotation();
}

FVector AMOISurfaceGraph::GetNormal() const
{
	return CachedFaceOrigin.GetUnitAxis(EAxis::Z);
}

FVector AMOISurfaceGraph::GetCorner(int32 index) const
{
	return ensure(CachedFacePoints.IsValidIndex(index)) ? CachedFacePoints[index] : GetLocation();
}

int32 AMOISurfaceGraph::GetNumCorners() const
{
	return CachedFacePoints.Num();
}

void AMOISurfaceGraph::PreDestroy()
{
	FaceIdxToVertexID.Reset();
	GraphFaceToInnerBound.Reset();
	GraphVertexToBoundVertex.Reset();
	CachedFacePoints.Reset();
}

bool AMOISurfaceGraph::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	int32 numVerts = GraphVertexToBoundVertex.Num();

	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		UModumateDocument* doc = GetDocument();
		if (!ensure(doc))
		{
			return false;
		}

		auto surfaceGraph = doc->FindSurfaceGraph(ID);
		if (!ensure(surfaceGraph.IsValid()) || !UpdateCachedGraphData())
		{
			return false;
		}

		auto hostObj = doc->GetObjectById(GetParentID());
		if (hostObj == nullptr)
		{
			return false;
		}

		AMOIMetaPlaneSpan* spanParent = Cast<AMOIMetaPlaneSpan>(doc->GetObjectById(hostObj->GetParentID()));
		if (spanParent == nullptr || spanParent->InstanceData.GraphMembers.Num() == 0)
		{
			return false;
		}

		// TODO: support surface graphs across all faces of a span
		auto faceObj = doc->GetObjectById(spanParent->InstanceData.GraphMembers[0]);

		if (faceObj == nullptr)
		{
			return false;
		}

		const auto graph = doc->FindVolumeGraph(faceObj->ID);
		const auto* face = graph ? graph->FindFace(faceObj->ID) : nullptr;
		if (face == nullptr)
		{
			return false;
		}

		// associate corners of the face with surface graph vertex IDs, if it 
		// hasn't happened yet
		int32 numIDs = FaceIdxToVertexID.Num();
		if (numIDs == 0)
		{
			// if the outer bounds don't exist yet, set them to be the root polygon
			// TODO: should the outer bounds always be the same as the root polygon?
			TArray<int32> boundingVertexIDs;
			surfaceGraph->GetOuterBoundsIDs(boundingVertexIDs);
			auto poly = surfaceGraph->FindPolygon(surfaceGraph->GetRootExteriorPolyID());

			if (poly != nullptr)
			{
				if (boundingVertexIDs.Num() == 0)
				{
					TArray<int32> emptyBounds;
					surfaceGraph->SetBounds(poly->ID, emptyBounds);
					surfaceGraph->GetOuterBoundsIDs(boundingVertexIDs);
				}

				// associate the outer bounds with the hosting object geometry
				for (int32 facePointIdx = 0; facePointIdx < CachedFacePoints.Num(); ++facePointIdx)
				{
					FVector2D prevPos2D = UModumateGeometryStatics::ProjectPoint2DTransform(CachedFacePoints[facePointIdx], CachedFaceOrigin);
					FGraph2DVertex* vertex = surfaceGraph->FindVertex(prevPos2D);
					if ((vertex) && poly->VertexIDs.Contains(vertex->ID))
					{
						FaceIdxToVertexID.Add(facePointIdx, vertex->ID);
					}
					else
					{
						bLinked = false;
					}
				}
			}
		}
		numIDs = FaceIdxToVertexID.Num();

		// If we aren't evaluating side effects, then updating cached graph data (and subsequently dirtying children surface graph elements) is enough
		if (OutSideEffectDeltas == nullptr)
		{
			CheckGraphLink();

			if (FVector::Parallel(face->CachedPlane, GetNormal()))
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
						const auto* containedFace = graph->FindFace(kvp.Key);

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

		// If the cached host face geometry has changed after it was created, then the surface graph may need to be updated or deleted to match the new host face
		if (numIDs > 0)
		{
			bool bFoundAllVertices = true;
			TMap<int32, FVector2D> vertexMoves;

			TArray<int32> boundingVertexIDs;
			surfaceGraph->GetOuterBoundsIDs(boundingVertexIDs);

			TArray<FDeltaPtr> validSideEffectDeltas;

			// For now, only attempt to generate new vertex movement positions for points that are part of the surface graph bounds,
			// and are included in the surface graph's mounting face point list
			int32 numCachedFacePoints = CachedFacePoints.Num();
			if (numCachedFacePoints != numIDs && bLinked)
			{
				bFoundAllVertices = false;
			}
			else 
			{
				if (!surfaceGraph->IsOuterBoundsDirty() && bLinked)
				{
					for (int32 facePointIdx = 0; facePointIdx < numCachedFacePoints; ++facePointIdx)
					{
						int32* index = FaceIdxToVertexID.Find(facePointIdx);
						if (!ensure(index != nullptr))
						{
							bFoundAllVertices = false;
							break;
						}

						FGraph2DVertex* vertex = surfaceGraph->FindVertex(*index);
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
			int32 nextID = doc->ReserveNextIDs(ID);
			
			// movement updates
			for (auto& kvp : GraphVertexToBoundVertex)
			{
				auto graphVertex = graph->FindVertex(kvp.Key);
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

			if (FVector::Parallel(face->CachedPlane, GetNormal()))
			{
				// removes
				TArray<FGraph2DDelta> deleteDeltas;
				TArray<int32> deleteIDs;
				TArray<int32> deleteFaceIDs;
				for (auto& kvp : GraphFaceToInnerBound)
				{
					// face in the map is no longer contained
					if (kvp.Key != MOD_ID_NONE && !face->ContainedFaceIDs.Contains(kvp.Key))
					{
						deleteIDs.Add(kvp.Value);
						deleteFaceIDs.Add(kvp.Key);
					}
				}
				for (int32 id : deleteFaceIDs)
				{
					GraphFaceToInnerBound.Remove(id);
				}

				surfaceGraph->DeleteObjects(deleteDeltas, nextID, deleteIDs, false);
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

				doc->FinalizeGraph2DDeltas(deleteDeltas, nextID, validSideEffectDeltas);

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
						doc->SetNextID(doc->GetNextAvailableID(), ID);
						return true;
					}
					doc->FinalizeGraph2DDeltas(boundsDeltas, nextID, validSideEffectDeltas);
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
				OutSideEffectDeltas->Append(validSideEffectDeltas);
				doc->FinalizeGraph2DDeltas(moveDeltas, nextID, *OutSideEffectDeltas);
			}
			// Otherwise, delete the surface graph if it cannot be preserved after the underlying geometry changes
			else
			{
				doc->GetDeleteObjectsDeltas(*OutSideEffectDeltas, { this });
			}

			doc->SetNextID(nextID, ID);
		}
		else
		{
			doc->GetDeleteObjectsDeltas(*OutSideEffectDeltas, { this });
		}
	}

	numVerts = GraphVertexToBoundVertex.Num();

	return true;
}

bool AMOISurfaceGraph::CheckGraphLink()
{
	UModumateDocument* doc = GetDocument();
	auto surfaceGraph = doc ? doc->FindSurfaceGraph(ID) : nullptr;
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
		int32* index = FaceIdxToVertexID.Find(facePointIdx);
		if (!ensure(index != nullptr))
		{
			bLinked = false;
			return false;
		}
		FGraph2DVertex* vertex = surfaceGraph->FindVertex(*index);
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

bool AMOISurfaceGraph::UpdateCachedGraphData()
{
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (!ensure(parentObj) || parentObj->IsDirty(EObjectDirtyFlags::Structure) || parentObj->IsDirty(EObjectDirtyFlags::Mitering))
	{
		return false;
	}

	return UModumateObjectStatics::GetGeometryFromFaceIndex(parentObj, InstanceData.ParentFaceIndex, CachedFacePoints, CachedFaceOrigin);
}

bool AMOISurfaceGraph::CalculateFaces(const TArray<int32>& AddIDs, TMap<int32, TArray<FVector2D>>& OutPolygonsToAdd, TMap<int32, TArray<int32>>& OutFaceToVertices)
{
	if (AddIDs.Num() == 0)
	{
		return false;
	}

	UModumateDocument* doc = GetDocument();
	if (!ensure(doc))
	{
		return false;
	}

	int32 hitFaceIndex = UModumateObjectStatics::GetParentFaceIndex(this);
	TArray<FVector> cornerPositions;
	FVector origin, normal, axisX, axisY;

	auto hostmoi = doc->GetObjectById(GetParentID());
	if (hostmoi && UModumateObjectStatics::GetGeometryFromFaceIndex(hostmoi, hitFaceIndex,
		cornerPositions, normal, axisX, axisY))
	{
		origin = cornerPositions[0];

		for (int32 id : AddIDs)
		{
			const auto* containedFace = doc->FindVolumeGraph(id)->FindFace(id);
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
