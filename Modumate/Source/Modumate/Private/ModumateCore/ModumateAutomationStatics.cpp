// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateAutomationStatics.h"


TArray<TArray<FVector>> UModumateAutomationStatics::GenerateProceduralGeometry(UWorld* World, int32 NextAvailableID, FGraph3D* Graph, int32 XIterations, int32 YIterations, TSharedRef<FGraph3DDelta> outGraphDelta, float XDim, float YDim, FVector PlaneOrigin, FVector PlaneDirection)
{

	TArray<TArray<FVector>> planesAndPoints;
	TArray<FVector> planePoints;

	FVector2D point1, point2, point3, point4;
	float xOffsetAccumulator = 0.0f;
	float yOffsetAccumulator = 0.0f;
	float xOffsetAccumulatorDynamic = 0.0f;
	float yOffsetAccumulatorDynamic = 0.0f;
	int32 graphIDCounter = NextAvailableID;
	int32 outID = 0;
	TSet<int32> groupIDs;
	FTransform planeTransform;
	planeTransform.SetLocation(PlaneOrigin);
	planeTransform.SetRotation(PlaneDirection.ToOrientationRotator().Quaternion());
	for (int gridYIndex = 0; gridYIndex < YIterations; gridYIndex++)
	{
		//determine the proper value to use for this pass depending on if it's a fractional remainder segment
		float yValue = YDim;
		for (int gridXIndex = 0; gridXIndex < XIterations; gridXIndex++)
		{
			float xValue = XDim;

			point1 = point2 = point3 = point4 = FVector2D::ZeroVector;
			planePoints.Empty();

			point1.X = xOffsetAccumulator;
			point1.Y = yOffsetAccumulator;

			point2.X = xValue + xOffsetAccumulator;
			point2.Y = yOffsetAccumulator;

			point3.X = xValue + xOffsetAccumulator;
			point3.Y = yValue + yOffsetAccumulator;

			point4.X = xOffsetAccumulator;
			point4.Y = yValue + yOffsetAccumulator;

			xOffsetAccumulator += xValue;

			FOccluderVertexArray facePoints;
			
			facePoints.Add(FVector(point1.X, 0, point1.Y));
			facePoints.Add(FVector(point2.X, 0, point2.Y));
			facePoints.Add(FVector(point3.X, 0, point3.Y));
			facePoints.Add(FVector(point4.X, 0, point4.Y));
			facePoints[0] = planeTransform.TransformPositionNoScale(facePoints[0]);
			facePoints[1] = planeTransform.TransformPositionNoScale(facePoints[1]);
			facePoints[2] = planeTransform.TransformPositionNoScale(facePoints[2]);
			facePoints[3] = planeTransform.TransformPositionNoScale(facePoints[3]);
			//accumulate deltas for vertices
			int32 vertID1, vertID2, vertID3, vertID4;


			Graph->GetDeltaForVertexAddition(facePoints[0], *outGraphDelta, graphIDCounter, vertID1);
			Graph->GetDeltaForVertexAddition(facePoints[1], *outGraphDelta, graphIDCounter, vertID2);
			Graph->GetDeltaForVertexAddition(facePoints[2], *outGraphDelta, graphIDCounter, vertID3);
			Graph->GetDeltaForVertexAddition(facePoints[3], *outGraphDelta, graphIDCounter, vertID4);

			//accumulate deltas for edge additions
			TArray<int32> outIDs;
			int32 existingID;
			Graph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID1, vertID2), *outGraphDelta, graphIDCounter, existingID, outIDs);
			outIDs.Reset();
			Graph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID2, vertID3), *outGraphDelta, graphIDCounter, existingID, outIDs);
			outIDs.Reset();
			Graph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID3, vertID4), *outGraphDelta, graphIDCounter, existingID, outIDs);
			outIDs.Reset();
			Graph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID4, vertID1), *outGraphDelta, graphIDCounter, existingID, outIDs);
			outIDs.Reset();

			//accumulate face addition delta
			TArray<int32> vertIDs;
			vertIDs.Add(vertID1);
			vertIDs.Add(vertID2);
			vertIDs.Add(vertID3);
			vertIDs.Add(vertID4);
			TMap<int32, int32> parentEdgeIdxToId;
			TArray<int32> parentFaceIDs;
			Graph->GetDeltaForFaceAddition(vertIDs, *outGraphDelta, graphIDCounter, existingID, parentFaceIDs, parentEdgeIdxToId, outID);

			//for displaying the debug dynamic mesh
			planesAndPoints.Add(facePoints);

		}
		xOffsetAccumulator = 0.0f;
		yOffsetAccumulator += yValue;
	}


	return planesAndPoints;
}