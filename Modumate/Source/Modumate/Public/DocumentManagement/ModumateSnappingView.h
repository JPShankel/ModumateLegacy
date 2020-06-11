// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Core/Public/Containers/Array.h"
#include "DocumentManagement/ModumateObjectInstance.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	class FModumateDocument;
	class FModumateObjectInstance;

	class MODUMATE_API FModumateSnappingView
	{
	private:
		FModumateDocument *Document;
		AEditModelPlayerController_CPP *Controller;

		TArray<FStructurePoint> CurObjCorners;
		TArray<FStructureLine> CurObjLineSegments;

		TArray<FVector> CurrentToolPoints;
		TArray<TPair<FVector, FVector>> CurrentToolLines;

	public:
		FModumateSnappingView(FModumateDocument *document, AEditModelPlayerController_CPP *controller);
		~FModumateSnappingView();

		TArray<FStructurePoint> Corners;
		TArray<FStructureLine> LineSegments;

		struct FSnapIndices
		{
			int32 CornerStartIndex;
			int32 NumCorners;
			int32 LineStartIndex;
			int32 NumLines;
		};

		TMap<int32, FSnapIndices> SnapIndicesByObjectID;

		void UpdateSnapPoints(const TSet<int32> &idsToIgnore, int32 collisionChannelMask = ~0, bool bForSnapping = false, bool bForSelection = false);

		// Get the points and lines for a bounding box.
		// Optionally constrain them to a single side of the box based on a unit axis.
		static void GetBoundingBoxPointsAndLines(const FVector &center, const FQuat &rot, const FVector &halfExtents,
			TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines,
			const FVector &OverrideAxis = FVector::ZeroVector);
	};
}
