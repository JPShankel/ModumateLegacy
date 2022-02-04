// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/GraphCommon.h"
#include "Graph/Graph2DTypes.h"
#include "Objects/ModumateObjectEnums.h"


static constexpr float DEFAULT_GRAPH_EPSILON = 0.5f;

enum class EGraphObjectType : uint8
{
	None,
	Vertex,
	Edge,
	Polygon
};

class FGraph2D;

class IGraph2DObject
{
public:
	IGraph2DObject(int32 InID, TWeakPtr<FGraph2D> InGraph);
	virtual ~IGraph2DObject();

public:
	// Mark the object's modified and derived data dirty flags as true, and potentially mark connected objects dirty as well
	virtual void Dirty(bool bConnected = true);

	// Update the object's derived data, clear the dirty flag, and return whether the flag had been set
	virtual bool Clean() = 0;

	// Clear the object modification flag, and return whether it had been set
	bool ClearModified();

	virtual EGraphObjectType GetType() const = 0;
	virtual void GetVertexIDs(TArray<int32> &OutVertexIDs) const = 0;

public:
	int32 ID = MOD_ID_NONE;			// The ID of the object, can match a corresponding MOI
	bool bValid = false;			// Whether this object can be used in the graph
	bool bModified = false;			// Whether this object has been modified since the last time this flag was cleared (for updating reflected MOIs)
	bool bDerivedDataDirty = false;	// Whether this object's definitional data has changed since its derived data has been updated

	TWeakPtr<FGraph2D> Graph;		// The graph that owns this object
};

