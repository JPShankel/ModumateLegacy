// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateTypes.h"

namespace Modumate
{

#define DEFAULT_GRAPH_EPSILON 0.05f

	// Use this typedef to designate edge IDs, which are only valid if non-0, and whose sign indicates a direction
	typedef int32 FEdgeID;

	enum EGraphObjectType
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
		IGraph2DObject(int32 InID, FGraph2D *InGraph) : ID(InID), Graph(InGraph) {};
		virtual ~IGraph2DObject() {}

	public:
		virtual void Dirty(bool bConnected = true) = 0;
		virtual void Clean() = 0;

		virtual EGraphObjectType GetType() const = 0;

	public:
		int32 ID = MOD_ID_NONE;		// The ID of the object, can match a corresponding MOI
		bool bValid = false;		// Whether this object can be used in the graph
		bool bDirty = false;		// Whether this object needs to be updated

		FGraph2D *Graph = nullptr;	// The graph that owns this object
	};
}
