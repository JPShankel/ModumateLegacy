// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DPolygon.h"

#include "Graph/Graph2D.h"

#include "ModumateCore/ModumateFunctionLibrary.h"

namespace Modumate
{
	bool FGraph2DPolygon::IsInside(const FGraph2DPolygon &otherPoly) const
	{
		// Polygons cannot be contained inside exterior polygons
		if (!otherPoly.bInterior)
		{
			return false;
		}

		// If we've already established the other poly as a parent, return the cached result
		if (ParentID == otherPoly.ID)
		{
			return true;
		}

		// If this is a parent of the other polygon, the parent cannot be contained within the child
		if (otherPoly.ParentID == ID)
		{
			return false;
		}

		// Early out by checking the AABBs
		if (!otherPoly.AABB.IsInside(AABB))
		{
			return false;
		}

		for (const FVector2D &point : Points)
		{
			if (UModumateFunctionLibrary::PointInPoly2D(point, otherPoly.Points))
			{
				return true;
			}
		}

		return false;
	}

	void FGraph2DPolygon::SetParent(int32 inParentID)
	{
		if (ParentID != inParentID)
		{
			// remove self from old parent
			if (ParentID != 0)
			{
				FGraph2DPolygon *parentPoly = Graph->FindPolygon(ParentID);
				if (ensureAlways(parentPoly))
				{
					int32 numRemoved = parentPoly->InteriorPolygons.Remove(ID);
					ensureAlways(numRemoved == 1);
				}
			}

			ParentID = inParentID;

			// add self to new parent
			if (ParentID != 0)
			{
				FGraph2DPolygon *parentPoly = Graph->FindPolygon(ParentID);
				if (ensureAlways(parentPoly && !parentPoly->InteriorPolygons.Contains(ID)))
				{
					parentPoly->InteriorPolygons.Add(ID);
				}
			}
		}
	}
}
