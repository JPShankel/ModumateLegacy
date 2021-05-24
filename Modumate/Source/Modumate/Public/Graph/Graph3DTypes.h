// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/GraphCommon.h"
#include "ModumateCore/ModumateTypes.h"

static constexpr float DEFAULT_GRAPH3D_EPSILON = 0.5f;

// Use this typedef to designate edge and face IDs, which are only valid if non-0, and whose sign indicates a direction
typedef TFunction<bool(FGraphSignedID SignedID)> FGraphObjPredicate;

enum class EGraph3DObjectType : uint8
{
	None,
	Vertex,
	Edge,
	Face,
	Polyhedron
};

struct FEdgeIntersection
{
	int32 EdgeIdxA;
	int32 EdgeIdxB;
	float DistAlongRay;
	float DistAlongEdge;
	FVector Point;

	bool operator<(const FEdgeIntersection &Other) const
	{
		return DistAlongRay < Other.DistAlongRay;
	}

	FEdgeIntersection(int32 InEdgeIdxA, int32 InEdgeIdxB, float InDistAlongRay, float InDistAlongEdge, const FVector &InPoint)
		: EdgeIdxA(InEdgeIdxA)
		, EdgeIdxB(InEdgeIdxB)
		, DistAlongRay(InDistAlongRay)
		, DistAlongEdge(InDistAlongEdge)
		, Point(InPoint)
	{ }
};

struct FGraph3DTraversal
{
	TArray<FGraphSignedID> FaceIDs;
	TArray<FVector> FacePoints;

	FGraph3DTraversal(const TArray<FGraphSignedID> &InFaceIDs, const TArray<FVector> &InFacePoints)
		: FaceIDs(InFaceIDs)
		, FacePoints(InFacePoints)
	{ }
};

class FGraph3D;

class IGraph3DObject
{
public:
	IGraph3DObject(int32 InID, FGraph3D *InGraph, const TSet<int32> &InGroupIDs) : ID(InID), Graph(InGraph), GroupIDs(InGroupIDs) {}
	virtual ~IGraph3DObject() {}

public:
	virtual void Dirty(bool bConnected = true) = 0;
	// Thorough validation function used as a success metric for unit tests
	virtual bool ValidateForTests() const = 0;

	virtual EGraph3DObjectType GetType() const = 0;

	virtual void GetVertexIDs(TArray<int32>& OutVertexIDs) const = 0;

public:
	int32 ID = MOD_ID_NONE; // The ID of the object, should match a corresponding MOI
	bool bValid = false;	// Whether this object can be used in the graph
	bool bDirty = false;	// Whether this object needs to be updated

	FGraph3D *Graph = nullptr; // The graph that owns this object

	TSet<int32> GroupIDs; // IDs of groups that contain this graph object
};
