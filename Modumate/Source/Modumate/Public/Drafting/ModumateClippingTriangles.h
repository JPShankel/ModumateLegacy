// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
class FModumateDocument;
class FModumateObjectInstance;

namespace Modumate
{

	class FModumateClippingTriangles
	{
	public:
		FModumateClippingTriangles(const FModumateObjectInstance& CutPlane);
		void SetTransform(FVector ViewPosition, FVector ViewXAxis, float ViewScale);
		void AddTrianglesFromDoc(const FModumateDocument* doc);
		TArray<FEdge> ClipWorldLineToView(FEdge line);
		TArray<FEdge> ClipViewLineToView(FEdge line);
		FEdge WorldLineToView(FEdge line) const;

	private:
		bool IsPointInFront(FVector Point) const;
		void BuildAccelerationStructure();
		FVector Position;
		FVector Normal;
		FMatrix TransformMatrix;
		float Scale;

		struct Occluder
		{
			FVector Vertices[3];
			FBox2D BoundingBox;
			float MinZ;
			float Area2D() const;
		};

		TArray<Occluder> Occluders;
		bool ClipSingleWorldLine(FEdge& viewLine, Occluder occluder, TArray<FEdge>& generatedLines);

		class QuadTreeNode
		{
		public:
			QuadTreeNode(const FBox2D& Box, int NodeDepth = 0);
			void AddOccluder(const Occluder* NewOccluder);
			// Apply a functor to all intersecting lines & boxes (until failure).
			bool Apply(const FEdge& line, TFunctionRef<bool (const Occluder& occluder)> functor);
			
			const FBox2D NodeBox;

		private:
			int NodeDepth { 0 };
			TArray<const Occluder*> Occluders;
			TUniquePtr<QuadTreeNode> Children[4];

			static const int MaxTreeDepth = 8;
		};

		TUniquePtr<QuadTreeNode> QuadTree;

		// Erode-length in world units.
		static constexpr float LineClipEpsilon = 0.05f;
	};

}
