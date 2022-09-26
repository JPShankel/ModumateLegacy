// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlaneSpan.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "DocumentManagement/ModumateDocument.h"

AMOIMetaPlaneSpan::AMOIMetaPlaneSpan()
	: AMOIPlaneBase()
{
}

const FGraph3DFace* AMOIMetaPlaneSpan::GetPerimeterFace() const
{
	return &CachedPerimeterFace;
}

bool AMOIMetaPlaneSpan::CanAdd(int32 FaceID) const
{
	if (InstanceData.GraphMembers.Num() == 0)
	{
		return true;
	}

	auto* graph = GetDocument()->FindVolumeGraph(FaceID);

	if (graph == nullptr || graph->GraphID != CachedGraphID)
	{
		return false;
	}

	const FGraph3DFace* faceOb = graph->FindFace(FaceID);

	if (!faceOb)
	{
		return false;
	}

	if (!UModumateGeometryStatics::ArePlanesCoplanar(faceOb->CachedPlane, GetPerimeterFace()->CachedPlane))
	{
		return false;
	}

	for (int32 edgeID : faceOb->EdgeIDs)
	{
		if (CachedEdgeIDs.Contains(edgeID) ||
			CachedEdgeIDs.Contains(edgeID * -1))
		{
			return true;
		}
	}

	return false;
}

void AMOIMetaPlaneSpan::PreDestroy()
{
	if (InstanceData.GraphMembers.Num() > 0)
	{
		auto* graphMoi = Document->GetObjectById(Document->FindGraph3DByObjID(InstanceData.GraphMembers[0]));
		if (graphMoi)
		{
			graphMoi->MarkDirty(EObjectDirtyFlags::Structure);
		}
		for (auto curMemberID : InstanceData.GraphMembers)
		{
			auto curMemberMoi = Document->GetObjectById(curMemberID);
			if (curMemberMoi)
			{
				curMemberMoi->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}
	}

	Super::PreDestroy();
}

TArray<int32> AMOIMetaPlaneSpan::GetFaceSpanMembers() const
{
	return InstanceData.GraphMembers;
}

#define ENFORCE_SINGLETON_SPANS

bool AMOIMetaPlaneSpan::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		//UE_LOG(LogTemp, Display, TEXT("Cleaning span %d with %d members"),ID,InstanceData.GraphMembers.Num());

		TArray<FDeltaPtr> outDeltas;

		if (PostGraphChanges.Num() > 0)
		{
			TArray<int32> additions, removals;
			FInstanceData newData = InstanceData;
			for (auto change : PostGraphChanges)
			{
				if (change < 0)
				{
					if (newData.GraphMembers.Contains(-change))
					{
						removals.Add(-change);
					}
				}
				else if (CanAdd(change))
				{
					additions.AddUnique(change);
				}
			}
			PostGraphChanges.Empty();
			FModumateObjectDeltaStatics::GetDeltasForFaceSpanAddRemove(Document, ID, additions, removals, outDeltas);
		}
#ifdef ENFORCE_SINGLETON_SPANS
		// TODO: remove for multi-face spans
		else if (InstanceData.GraphMembers.Num() > 1)
		{
			FModumateObjectDeltaStatics::GetDeltasForSpanSplit(Document, { ID }, outDeltas);
		}
#endif
		else if (InstanceData.GraphMembers.Num() == 0 || GetChildIDs().Num() == 0)
		{
			Document->GetDeleteObjectsDeltas(outDeltas, { this }, true, true);
		}
		else if (!UpdateCachedPerimeterFace())
		{
			FModumateObjectDeltaStatics::GetDeltasForSpanSplit(Document, { ID }, outDeltas);
		}

		if (OutSideEffectDeltas)
		{
			OutSideEffectDeltas->Append(outDeltas);
		}
	}
	return true;
}

void AMOIMetaPlaneSpan::SetupDynamicGeometry()
{
	UpdateCachedPerimeterFace();
	Super::SetupDynamicGeometry();
}

bool AMOIMetaPlaneSpan::UpdateCachedPerimeterFace()
{
	// The perimeter face is declared by value and not a member of the graph itself
	CachedPerimeterFace = FGraph3DFace();
	bool bLegal = true;

	if (InstanceData.GraphMembers.Num() == 0)
	{
		return false;
	}

	FGraph3D* graph = GetDocument()->FindVolumeGraph(InstanceData.GraphMembers[0]);
	int32 graphIdx = 1;
	while (!graph && graphIdx < InstanceData.GraphMembers.Num())
	{
		graph = GetDocument()->FindVolumeGraph(InstanceData.GraphMembers[graphIdx++]);
	}

	if (!graph)
	{
		return false;
	}

	CachedGraphID = graph->GraphID;

	// Dirty metagraph MOI for bounding box.
	AModumateObjectInstance* graphOb = Document->GetObjectById(CachedGraphID);
	if (ensure(graphOb))
	{
		graphOb->MarkDirty(EObjectDirtyFlags::Structure);
	}

	TArray<const FGraph3DFace* > memberFaces;
	Algo::Transform(InstanceData.GraphMembers, memberFaces,
		[graph](int32 FaceID) {return graph->FindFace(FaceID); });

	if (memberFaces.Num() == 0)
	{
		return true;
	}

	FPlane basePlane  = memberFaces[0]->CachedPlane;

#ifdef ENFORCE_SINGLETON_SPANS
	CachedPerimeterFace = *memberFaces[0];

	// Update AMOIPlaneBase cached values
	CachedPoints = CachedPerimeterFace.CachedPositions;
	CachedPlane = CachedPerimeterFace.CachedPlane;
	CachedAxisX = CachedPerimeterFace.Cached2DX;
	CachedAxisY = CachedPerimeterFace.Cached2DY;
	CachedOrigin = CachedPerimeterFace.CachedPositions.Num() > 0 ? CachedPerimeterFace.CachedPositions[0] : FVector::ZeroVector;
	CachedCenter = CachedPerimeterFace.CachedCenter;
	CachedHoles = CachedPerimeterFace.CachedHoles;
	CachedEdgeIDs = TSet<int32>(CachedPerimeterFace.EdgeIDs);

	return true;

#endif

	auto numFacesForEdge = [this, &memberFaces](int32 EdgeID)
	{
		int32 numFaces = 0;
		for (auto& face : memberFaces)
		{
			bool bSameDirection = false;
			if (face && face->FindEdgeIndex(EdgeID,bSameDirection) != INDEX_NONE)
			{
				++numFaces;
			}
		}
		return numFaces;
	};

	// Gather all the perimeter edges into an unsorted list
	TArray<int32> perimeterEdges;
	for (auto* face: memberFaces)
	{
		if (!face)
		{
			continue;
		}
		// Edges are perimeter if they have only one face in the member list
		for (auto edgeID : face->EdgeIDs)
		{
			if (numFacesForEdge(edgeID) == 1)
			{
				perimeterEdges.Add(edgeID);
			}
		}
		CachedPerimeterFace.CachedHoles.Append(face->CachedHoles);
		CachedPerimeterFace.Cached2DHoles.Append(face->Cached2DHoles);

		if (!UModumateGeometryStatics::ArePlanesCoplanar(basePlane, face->CachedPlane))
		{
			bLegal = false;
		}
	}

	auto findConnectingEdge = [this,graph,&perimeterEdges](int32 EdgeID)
	{
		const FGraph3DEdge* thisEdge = graph->FindEdge(EdgeID);
		if (!graph)
		{
			return MOD_ID_NONE;
		}
		for (auto edgeID : perimeterEdges)
		{
			if (edgeID == EdgeID)
			{
				continue;
			}
			const FGraph3DEdge* otherEdge = graph->FindEdge(edgeID);

			if (otherEdge->StartVertexID == thisEdge->StartVertexID ||
				otherEdge->EndVertexID == thisEdge->StartVertexID ||
				otherEdge->StartVertexID == thisEdge->EndVertexID ||
				otherEdge->EndVertexID == thisEdge->EndVertexID)
			{
				return edgeID;
			}
		}
		return MOD_ID_NONE;
	};

	// Now, starting with the first edge, walk around finding connecting edges in sequence
	// When we hit a duplicate, the perimeter is done
	// Note: if we ever need to support spans with detached faces, we'll need a new approach

	int32 edgeID = perimeterEdges[0];

	//The cached perimeter face now has edges laid out in order
	while (edgeID != 0)
	{
		perimeterEdges.Remove(edgeID);
		CachedPerimeterFace.EdgeIDs.Add(edgeID);
		edgeID = findConnectingEdge(edgeID);
	}

	// Now walk around the edges and gather vertices, positions and normals
	for (auto perimeterEdge : CachedPerimeterFace.EdgeIDs)
	{
		const FGraph3DEdge* edgeOb = graph->FindEdge(perimeterEdge);
		// Negative edge IDs indicate reverse direction
		int32 vertID = perimeterEdge > 0 ? edgeOb->StartVertexID : edgeOb->EndVertexID;
		const FGraph3DVertex* vertexOb = graph->FindVertex(perimeterEdge > 0 ? edgeOb->StartVertexID : edgeOb->EndVertexID);

		CachedPerimeterFace.VertexIDs.Add(vertID);
		CachedPerimeterFace.CachedPositions.Add(graph->FindVertex(vertID)->Position);
	}

	// And analyze the derived values
	UModumateGeometryStatics::AnalyzeCachedPositions(CachedPerimeterFace.CachedPositions, CachedPerimeterFace.CachedPlane,
		CachedPerimeterFace.Cached2DX, CachedPerimeterFace.Cached2DY, CachedPerimeterFace.Cached2DPositions, CachedPerimeterFace.CachedCenter, true);

	for (auto perimeterEdge : CachedPerimeterFace.EdgeIDs)
	{
		const FGraph3DEdge* edgeOb = graph->FindEdge(perimeterEdge);
		// Negative edge IDs indicate reverse direction
		int32 vertID = perimeterEdge > 0 ? edgeOb->StartVertexID : edgeOb->EndVertexID;
		const FGraph3DVertex* vertexOb = graph->FindVertex(vertID);

		FVector edgeDir = edgeOb->CachedDir*(edgeOb->ID > 0 ? 1.0f : -1.0f);
		FVector edgeNormal = CachedPlane ^ edgeDir;
		edgeNormal.Normalize();
		CachedPerimeterFace.CachedEdgeNormals.Add(edgeNormal);

		// Also cache the 2D edge normal here, which should already be safe to project into the plane and stay normalized.
		FVector2D edgeNormal2D = UModumateGeometryStatics::ProjectVector2D(edgeNormal, CachedPerimeterFace.Cached2DX, CachedPerimeterFace.Cached2DY);
		bool edgeNormal2DNormalized = (FMath::Abs(1.f - edgeNormal2D.SizeSquared()) < THRESH_VECTOR_NORMALIZED);
		CachedPerimeterFace.Cached2DEdgeNormals.Add(edgeNormal2D);
	}

	// Update AMOIPlaneBase cached values
	CachedPoints = CachedPerimeterFace.CachedPositions;
	CachedPlane = CachedPerimeterFace.CachedPlane;
	CachedAxisX = CachedPerimeterFace.Cached2DX;
	CachedAxisY = CachedPerimeterFace.Cached2DY;
	CachedOrigin = CachedPerimeterFace.CachedPositions.Num() > 0 ? CachedPerimeterFace.CachedPositions[0] : FVector::ZeroVector;
	CachedCenter = CachedPerimeterFace.CachedCenter;
	CachedHoles = CachedPerimeterFace.CachedHoles;
	CachedEdgeIDs = TSet<int32>(CachedPerimeterFace.EdgeIDs);

	Document->GetObjectById(graph->GraphID)->MarkDirty(EObjectDirtyFlags::Structure);

	return bLegal && CheckIsConnected();

}

bool AMOIMetaPlaneSpan::CheckIsConnected() const
{
	// Empty graphs are not connected, single element graphs are
	switch (InstanceData.GraphMembers.Num())
	{
		case 0: return false;
		case 1: return true;
	}

	// Grab the first face, add its edges to the set of all found edges, then find every other face and add their edges
	// If you run out of faces, the graph is connected...if any faces are orphaned, you don't
	TArray<int32> allFaces = InstanceData.GraphMembers;
	TSet<int32> foundEdges;

	const FGraph3D* graph = GetDocument()->FindVolumeGraph(InstanceData.GraphMembers[0]);
	int32 graphIdx = 1;
	while (graph == nullptr && graphIdx < InstanceData.GraphMembers.Num())
	{
		graph = GetDocument()->FindVolumeGraph(InstanceData.GraphMembers[graphIdx++]);
	}

	auto addEdges = [&foundEdges, this, graph](int32 FaceID)
	{
		const FGraph3DFace* faceOb = graph->FindFace(FaceID);
		if (faceOb)
		{
			foundEdges.Append(faceOb->EdgeIDs);
		}
	};

	auto faceConnected = [&foundEdges, this, graph](int32 FaceID)
	{
		const FGraph3DFace* faceOb = graph->FindFace(FaceID);
		if (faceOb)
		{
			for (int32 edgeID : faceOb->EdgeIDs)
			{
				if (foundEdges.Contains(edgeID) || foundEdges.Contains(-edgeID))
				{
					return true;
				}
			}
		}
		return false;
	};

	addEdges(allFaces.Pop());
	bool bContinue = true;
	do 
	{
		int32 idx = 0;
		int32 startingCount = allFaces.Num();
		bContinue = false;
		while (idx < allFaces.Num())
		{
			if (faceConnected(allFaces[idx]))
			{
				addEdges(allFaces[idx]);
				allFaces.RemoveAt(idx);
				bContinue = true;
			}
			else 
			{
				++idx;
			}
		}

	} while (bContinue);

	return allFaces.Num() == 0;
}

