// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ModumateGraph3DTypes.h"

namespace Modumate
{
	class FGraph3DPolyhedron : public IGraph3DObject
	{
	public:
		int32 ParentID = MOD_ID_NONE;		// The ID of the polyhedron that contains this one, if any
		TArray<int32> InteriorPolyhedra;	// The IDs of polyhedrons that this polyhedron contains
		TArray<FSignedID> FaceIDs;			// The list of signed faces that make up this polyhedron

		// Cached derived data
		bool bClosed = false;				// Whether this is a closed polyhedron (it fully bounds a volume)
		bool bInterior = false;				// Whether this is an interior polyhedron; otherwise it is a exterior shell or unclosed
		bool bConvex = true;				// Whether this polyhedron is convex
		FBox AABB = FBox(ForceInitToZero);	// The axis-aligned bounding box for the polyhedron

		FGraph3DPolyhedron(int32 InID, FGraph3D* InGraph, const TSet<int32> &InGroupIDs = TSet<int32>());

		bool IsInside(const FGraph3DPolyhedron &otherVolume) const;
		void SetParent(int32 inParentID);
		bool DetermineInterior();				// A helper function to determine whether this polyhedron is interior or exterior, if it is closed
		bool DetermineConvex();

		TArray<TArray<FSignedID>> GroupFacesByNormal(); // A helper function to find the most common face directions for use as drawing cuts

		virtual void Dirty(bool bConnected = true) override { }
		virtual bool ValidateForTests() const override { return true; }
		virtual EGraph3DObjectType GetType() const override { return EGraph3DObjectType::Polyhedron; }
	};
}
