#include "Graph/Graph3DPolyhedron.h"
#include "Graph/Graph3D.h"

FGraph3DPolyhedron::FGraph3DPolyhedron(int32 InID, FGraph3D* InGraph, const TSet<int32> &InGroupIDs)
	: IGraph3DObject(InID, InGraph, InGroupIDs)
	, ParentID(MOD_ID_NONE)
	, bClosed(false)
	, bInterior(false)
	, AABB(ForceInitToZero)
{
}

bool FGraph3DPolyhedron::IsInside(const FGraph3DPolyhedron &otherVolume) const
{
	// Polyhedra cannot be contained inside exterior polyhedra
	if (!otherVolume.bInterior)
	{
		return false;
	}

	// If we've already established the other polyhedron as a parent, return the cached result
	if (ParentID == otherVolume.ID)
	{
		return true;
	}

	// If this is a parent of the other polyhedron, the parent cannot be contained within the child
	if (otherVolume.ParentID == ID)
	{
		return false;
	}

	// Early out by checking the AABBs
	if (!otherVolume.AABB.IsInside(AABB))
	{
		return false;
	}

	// TODO: point-in-volume checks; probably requires convex decomp and half-plane comparisons
	return otherVolume.AABB.IsInside(AABB);
}

void FGraph3DPolyhedron::SetParent(int32 inParentID)
{
	if (ParentID != inParentID)
	{
		// remove self from old parent
		if (ParentID != 0)
		{
			FGraph3DPolyhedron *parentPolyhedron = Graph->FindPolyhedron(ParentID);
			if (ensureAlways(parentPolyhedron))
			{
				int32 numRemoved = parentPolyhedron->InteriorPolyhedra.Remove(ID);
				ensureAlways(numRemoved == 1);
			}
		}

		ParentID = inParentID;

		// add self to new parent
		if (ParentID != 0)
		{
			FGraph3DPolyhedron *parentPoly = Graph->FindPolyhedron(ParentID);
			if (ensureAlways(parentPoly && !parentPoly->InteriorPolyhedra.Contains(ID)))
			{
				parentPoly->InteriorPolyhedra.Add(ID);
			}
		}
	}
}

bool FGraph3DPolyhedron::DetermineInterior()
{
	int32 numFaces = FaceIDs.Num();
	if (!bClosed || (numFaces < 4))
	{
		return false;
	}

	for (FGraphSignedID sourceFaceID : FaceIDs)
	{
		bool bFaceHitsAnotherFace = false;
		float sourceSign = FMath::Sign(sourceFaceID);

		for (FGraphSignedID destFaceID : FaceIDs)
		{
			if (sourceFaceID == destFaceID)
			{
				continue;
			}

			const FGraph3DFace *sourceFace = Graph->FindFace(sourceFaceID);
			const FGraph3DFace *destFace = Graph->FindFace(destFaceID);
			if (!ensureAlways(sourceFace && destFace))
			{
				return false;
			}

			if (sourceFace->NormalHitsFace(destFace, sourceSign))
			{
				bFaceHitsAnotherFace = true;
				break;
			}
		}

		// If a face's normal doesn't hit any other face in the polyhedron,
		// then this traversal must be the exterior one.
		if (!bFaceHitsAnotherFace)
		{
			bInterior = false;
			return true;
		}
	}

	// Otherwise, if every face's normal hits another one of its faces, then it's interior.
	// TODO: this is not necessarily comprehensive; there may be concave shapes where every
	// face has a ray that hits another face and none escape, which would require a different test
	// (such as hitting an even number of faces), but this is much more expensive for marginal improvement.
	bInterior = true;
	return true;
}

bool FGraph3DPolyhedron::DetermineConvex()
{
	int32 numFaces = FaceIDs.Num();
	if (!bClosed || (numFaces < 4))
	{
		return false;
	}

	for (FGraphSignedID sourceFaceID : FaceIDs)
	{
		float sourceSign = FMath::Sign(sourceFaceID);
		if (!bInterior)
		{
			sourceSign *= -1.0f;
		}

		for (FGraphSignedID destFaceID : FaceIDs)
		{
			if (sourceFaceID == destFaceID)
			{
				continue;
			}

			const FGraph3DFace *sourceFace = Graph->FindFace(sourceFaceID);
			const FGraph3DFace *destFace = Graph->FindFace(destFaceID);
			if (!ensureAlways(sourceFace && destFace))
			{
				return false;
			}

			// If any of the other faces' points are behind a face, then the polyhedron is concave.
			FPlane sourcePlane = sourceFace->CachedPlane * sourceSign;
			for (const FVector &destPoint : destFace->CachedPositions)
			{
				if (sourcePlane.PlaneDot(destPoint) < -Graph->Epsilon)
				{
					bConvex = false;
					return true;
				}
			}
		}
	}

	bConvex = true;
	return true;
}

TArray<TArray<FGraphSignedID>> FGraph3DPolyhedron::GroupFacesByNormal()
{
	TSet<FGraphSignedID> visitedFaces;
	TArray<TArray<FGraphSignedID>> facesGroupedByNormal;

	for (int32 faceIdx = 0; faceIdx < FaceIDs.Num(); faceIdx++)
	{
		FGraphSignedID faceID = FaceIDs[faceIdx];
		FGraph3DFace* face = Graph->FindFace(faceID);
		FVector faceNormal = FVector(face->CachedPlane);

		if (visitedFaces.Contains(FMath::Abs(faceID)))
		{
			continue;
		}

		// currently only being used for vertical cuts 
		if (!FMath::IsNearlyZero(faceNormal.Z))
		{
			continue;
		}

		TArray<FGraphSignedID> currentGroup = { faceID };
		visitedFaces.Add(FMath::Abs(faceID));

		for (int32 otherFaceIdx = faceIdx + 1; otherFaceIdx < FaceIDs.Num(); otherFaceIdx++)
		{
			FGraphSignedID otherFaceID = FaceIDs[otherFaceIdx];
			FGraph3DFace* otherFace = Graph->FindFace(otherFaceID);
			FVector otherFaceNormal = FVector(otherFace->CachedPlane);

			if (!visitedFaces.Contains(FMath::Abs(otherFaceID)) && FVector::Parallel(faceNormal, otherFaceNormal))
			{
				currentGroup.Add(otherFaceID);
				visitedFaces.Add(FMath::Abs(otherFaceID));
			}
		}

		facesGroupedByNormal.Add(currentGroup);
	}

	return facesGroupedByNormal;
}

void FGraph3DPolyhedron::GetVertexIDs(TArray<int32>& OutVertexIDs) const
{
	TSet<int32> vertexIDs;

	for (int32 faceID : FaceIDs)
	{
		auto face = Graph->FindFace(faceID);
		if (face == nullptr)
		{
			continue;
		}
		vertexIDs.Append(face->VertexIDs);
	}

	OutVertexIDs = vertexIDs.Array();
}

