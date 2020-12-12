// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SegmentTypes.h"

class UModumateDocument;
class AModumateObjectInstance;

namespace Modumate
{

	class FModumateClippingTriangles
	{
	public:
		FModumateClippingTriangles(const AModumateObjectInstance& CutPlane);
		void SetTransform(FVector ViewPosition, FVector ViewXAxis, float ViewScale);
		void AddTrianglesFromDoc(const UModumateDocument* doc);
		TArray<FEdge> ClipWorldLineToView(FEdge line);
		TArray<FEdge> ClipViewLineToView(FEdge line);
		FEdge WorldLineToView(FEdge line) const;

		// Debugging:
		void GetTriangleEdges(TArray<FEdge>& outEdges) const;

	private:
		bool IsPointInFront(FVector Point) const;
		void BuildAccelerationStructure();
		FVector3d Position;
		FVector3d Normal;
		FMatrix TransformMatrix;
		float Scale;

		struct Occluder
		{
			FVector3d Vertices[3];
			FBox2D BoundingBox;
			double MinZ;
			double Area2D() const;
		};

		TArray<Occluder> Occluders;
		bool ClipSingleWorldLine(FSegment3d& viewLine, Occluder occluder, TArray<FSegment3d>& generatedLines);

		class QuadTreeNode
		{
		public:
			QuadTreeNode(const FBox2D& Box, int NodeDepth = 0);
			void AddOccluder(const Occluder* NewOccluder);
			// Apply a functor to all intersecting lines & boxes (until failure).
			bool Apply(const FSegment3d& line, TFunctionRef<bool (const Occluder& occluder)> functor);
			
			const FBox2D NodeBox;

		private:
			int NodeDepth { 0 };
			TArray<const Occluder*> Occluders;
			TUniquePtr<QuadTreeNode> Children[4];

			static const int MaxTreeDepth = 8;
		};

		TUniquePtr<QuadTreeNode> QuadTree;

		// Erode-length in world units.
		static constexpr double LineClipEpsilon = 0.05;
	};

}
