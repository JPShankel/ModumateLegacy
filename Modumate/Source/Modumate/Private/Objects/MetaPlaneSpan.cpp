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

bool AMOIMetaPlaneSpan::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		if (OutSideEffectDeltas)
		{
			if (GetChildIDs().Num() == 0 || InstanceData.GraphMembers.Num() == 0)
			{
				TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
				delta->AddCreateDestroyState(StateData, EMOIDeltaType::Destroy);
				OutSideEffectDeltas->Add(delta);
				return true;
			}
			else if (!UpdateCachedPerimeterFace())
			{
				FModumateObjectDeltaStatics::GetDeltasForSpanSplit(Document, { ID }, *OutSideEffectDeltas);
				return true;
			}
		}
	}
	return AMOIPlaneBase::CleanObject(DirtyFlag, OutSideEffectDeltas);
}

void AMOIMetaPlaneSpan::SetupDynamicGeometry()
{
	if (!UpdateCachedPerimeterFace())
	{
		MarkDirty(EObjectDirtyFlags::Structure);
	}
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

	if (!graph)
	{
		return false;
	}

	TArray<const FGraph3DFace* > memberFaces;
	Algo::Transform(InstanceData.GraphMembers, memberFaces,
		[graph](int32 FaceID) {return graph->FindFace(FaceID); });

	if (memberFaces.Num() == 0)
	{
		return false;
	}

	FPlane basePlane = memberFaces[0]->CachedPlane;

	auto numFacesForEdge = [this, &memberFaces](int32 EdgeID)
	{
		int32 numFaces = 0;
		for (auto& face : memberFaces)
		{
			bool bSameDirection = false;
			if (face->FindEdgeIndex(EdgeID,bSameDirection) != INDEX_NONE)
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
		return 0;
	};

	// Now, starting with the first edge, walk around finding connecting edges in sequence
	// When we hit a duplicate, the perimeter is done
	// Note: if we ever need to support spans with detached faces, we'll need a new approach

	int32 edgeID = perimeterEdges.Pop();

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
		CachedPerimeterFace.CachedEdgeNormals.Add(edgeOb->CachedRefNorm);
	}

	// And analyze the derived values
	UModumateGeometryStatics::AnalyzeCachedPositions(CachedPerimeterFace.CachedPositions, CachedPerimeterFace.CachedPlane,
		CachedPerimeterFace.Cached2DX, CachedPerimeterFace.Cached2DY, CachedPerimeterFace.Cached2DPositions, CachedPerimeterFace.CachedCenter, true);

	// Update AMOIPlaneBase cached values
	CachedPoints = CachedPerimeterFace.CachedPositions;
	CachedPlane = CachedPerimeterFace.CachedPlane;
	CachedAxisX = CachedPerimeterFace.Cached2DX;
	CachedAxisY = CachedPerimeterFace.Cached2DY;
	CachedOrigin = CachedPerimeterFace.CachedPositions.Num() > 0 ? CachedPerimeterFace.CachedPositions[0] : FVector::ZeroVector;
	CachedCenter = CachedPerimeterFace.CachedCenter;
	CachedHoles = CachedPerimeterFace.CachedHoles;

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

