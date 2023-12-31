#include "Graph/Graph3DTypes.h"

#pragma once

class FGraph3DVertex : public IGraph3DObject
{
public:
	FVector Position = FVector::ZeroVector;	// The position of the vertex
	TArray<FGraphSignedID> ConnectedEdgeIDs;		// The set of IDs of connected edges connected to this vertex, unsorted

	FGraph3DVertex(int32 InID, FGraph3D* InGraph, const FVector &InPos);

	void AddEdge(FGraphSignedID EdgeID);
	bool RemoveEdge(FGraphSignedID EdgeID);

	void GetConnectedFaces(TSet<int32>& OutFaceIDs) const;
	void GetConnectedFacesAndEdges(TSet<int32>& OutFaceIDs, TSet<int32>& OutEdgeIDs) const;

	virtual void Dirty(bool bConnected = true) override;
	virtual bool ValidateForTests() const override;
	virtual EGraph3DObjectType GetType() const override { return EGraph3DObjectType::Vertex; }

	virtual void GetVertexIDs(TArray<int32>& OutVertexIDs) const override;
};

