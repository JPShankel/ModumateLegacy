// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Drafting/ModumateOccluder.h"

class UModumateDocument;
class AModumateObjectInstance;
class FModumateViewLineSegment;

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
		bool IsBoxOccluded(const FBox2D& Box, float Depth) const;
		bool IsBoxOccluded(const FBox& Box) const;

		// Debugging:
		void GetTriangleEdges(TArray<FEdge>& outEdges) const;

	private:
		bool IsPointInFront(FVector Point) const;
		void BuildAccelerationStructure();
		FVector3d Position;
		FVector3d Normal;
		FMatrix TransformMatrix;
		float Scale;


		TArray<FModumateOccluder> Occluders;
		bool ClipSingleWorldLine(FModumateViewLineSegment& viewLine, FModumateOccluder occluder, TArray<FModumateViewLineSegment>& generatedLines);
		static bool IsBoxUnoccluded(const FModumateOccluder& Occluder, const FBox2D Box, float Depth);

		class QuadTreeNode
		{
		public:
			QuadTreeNode(const FBox2D& Box, int NodeDepth = 0);
			void AddOccluder(const FModumateOccluder* NewOccluder);
			// Apply a functor to all intersecting lines & boxes (until failure).
			bool Apply(const FModumateViewLineSegment& line, TFunctionRef<bool (const FModumateOccluder& occluder)> functor);
			bool Apply(const FBox2D& box, TFunctionRef<bool (const FModumateOccluder& occluder)> functor);
			
			const FBox2D NodeBox;

		private:
			int NodeDepth { 0 };
			TArray<const FModumateOccluder*> Occluders;
			TUniquePtr<QuadTreeNode> Children[4];

			static const int MaxTreeDepth = 8;
		};

		TUniquePtr<QuadTreeNode> QuadTree;

		// Erode-length in world units.
		static constexpr double LineClipEpsilon = 0.05;
	};

}
