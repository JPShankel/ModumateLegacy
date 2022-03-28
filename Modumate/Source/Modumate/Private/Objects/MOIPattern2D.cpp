// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/MOIPattern2D.h"
#include "Objects/MetaPlane.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
AMOIPattern2D::AMOIPattern2D()
{}

void AMOIPattern2D::SetupDynamicGeometry()
{
	UpdateDynamicGeometry();
}

//TODO: rename this function to have a more accurate name
bool AMOIPattern2D::GetDistanceBetweenIntersectingEdgesOfParentGraph(FVector2D StartSegment, FVector2D EndSegment, FVector2D& OutIntersect1, FVector2D& OutIntersect2)
{
	const AMOIMetaPlane* parent = Cast<AMOIMetaPlane>(GetParentObject());
	bool bStartSet = false;
	FVector start, end;
	if (parent != nullptr)
	{
		const FGraph3DFace* parentFace = Document->GetVolumeGraph()->FindFace(GetParentObject()->ID);
		FVector startSeg3D(StartSegment.X, StartSegment.Y, 0);
		FVector endSeg3D(EndSegment.X, EndSegment.Y, 0);

		//assume only 2 edges can intersect with segment
		for (int i = 0; i < parentFace->EdgeIDs.Num(); i++)
		{
			//need to add check for colinearity
			FGraph3DEdge *edge = Document->GetVolumeGraph()->FindEdge(parentFace->EdgeIDs[i]);
			FGraph3DVertex* startVert = Document->GetVolumeGraph()->FindVertex(edge->StartVertexID);
			FGraph3DVertex* endVert = Document->GetVolumeGraph()->FindVertex(edge->EndVertexID);

			FVector2D startVert2D = parentFace->ProjectPosition2D(startVert->Position);
			FVector2D endVert2D = parentFace->ProjectPosition2D(endVert->Position);

			FVector convertedStartVert3D(startVert2D.X, startVert2D.Y, 0);
			FVector convertedEndVert3D(endVert2D.X, endVert2D.Y, 0);
			FVector OutIntersectionPoint;
			
			if (FMath::SegmentIntersection2D(startSeg3D, endSeg3D, convertedStartVert3D, convertedEndVert3D, OutIntersectionPoint))
			{
				if (!bStartSet)
				{
					start = OutIntersectionPoint;
					bStartSet = true;
				}
				else {
					end = OutIntersectionPoint;
					OutIntersect1.X = start.X;
					OutIntersect1.Y = start.Y;
					OutIntersect2.X = end.X;
					OutIntersect2.Y = end.Y;
					return true;
				}
			}
		}
	}
	return false;
}

void AMOIPattern2D::UpdateDynamicGeometry()
{
	Super::SetupDynamicGeometry();
	DynamicMeshActor->ClearProceduralMesh();
	
	//this variable is used to quarantine the dynamic calculations which calculate the lengths and positions based on the parent face.
	bool bCalculateDynamic = false;
	if (CachedEssentialElementGraph == nullptr)
	{
		CachedEssentialElementGraph = MakeShareable(new FGraph3D());
	}
	else if (bGraphDirty)
	{
		CachedEssentialElementGraph->Reset();
	}
		
	// TODO: initial visual is just the hosted plane, future work: schematic layout of pattern
	const AMOIMetaPlane* parent = Cast<AMOIMetaPlane>(GetParentObject());
	if (parent != nullptr)
	{
		const FGraph3DFace *parentFace = Document->GetVolumeGraph()->FindFace(GetParentObject()->ID);

		float xDimension, yDimension;

		//making a lot of assumptions here to get the dimensions from the points, can probably get the dimensions from the meta plane.
		FBox2D boundsBox2D = FBox2D(parentFace->Cached2DPositions);
		xDimension = boundsBox2D.Max.X - boundsBox2D.Min.X;
		yDimension = boundsBox2D.Max.Y - boundsBox2D.Min.Y;
		
		float xAxisLength = 0.0f;
		float yAxisLength = 0.0f;
		int32 xFlexTotalWeight = 0;
		int32 yFlexTotalWeight = 0;
		FBIMPatternSequence xPatternElements, yPatternElements;
		xPatternElements = CachedAssembly.PatternData.SequenceX;
		yPatternElements = CachedAssembly.PatternData.SequenceY;
		//calculate essential element x length
		for (FBIMPatternSegment& patternAxisDataX : xPatternElements.Segments)
		{
			//if any regions are flex, the length becomes the plane length
			if (patternAxisDataX.SegmentType == EBIMPatternSegment::FractionalRemainder)
			{
				xFlexTotalWeight += patternAxisDataX.Value;
			}
			else {
				xAxisLength += patternAxisDataX.Value;
			}
		}
		//calculate essential element y length
		for (FBIMPatternSegment& patternAxisDataY : yPatternElements.Segments)
		{
			//if any regions are flex, the length becomes the plane length
			if (patternAxisDataY.SegmentType == EBIMPatternSegment::FractionalRemainder)
			{
				yFlexTotalWeight += patternAxisDataY.Value;
			}
			else {
				yAxisLength += patternAxisDataY.Value;
			}
		}

		//Calculate flex element magnitudes and update magnitudes in pattern definition
		float xFlexElementMagnitudeTotal = 0.0f;
		float yFlexElementMagnitudeTotal = 0.0f;
		if (xFlexTotalWeight > 0)
		{
			xFlexElementMagnitudeTotal = (xDimension - xAxisLength); //xDimension is total axis length, xAxisLength is total of absolute elements
			for (int32 i = 0; i < xPatternElements.Segments.Num(); i++)
			{
				if (xPatternElements.Segments[i].SegmentType == EBIMPatternSegment::FractionalRemainder)
				{
					xPatternElements.Segments[i].CachedFractionalRemainerFractionOfTotal = (float)xPatternElements.Segments[i].Value / (float)xFlexTotalWeight;
					xPatternElements.Segments[i].CachedFractionalRemainderMagnitude = xFlexElementMagnitudeTotal * xPatternElements.Segments[i].CachedFractionalRemainerFractionOfTotal;
				}
			}
		}
		if (yFlexTotalWeight > 0)
		{
			yFlexElementMagnitudeTotal = (yDimension - yAxisLength); //yDimension is total axis length, yAxisLength is total of absolute elements
			for (int32 i = 0; i < yPatternElements.Segments.Num(); i++)
			{
				if (yPatternElements.Segments[i].SegmentType == EBIMPatternSegment::FractionalRemainder)
				{
					yPatternElements.Segments[i].CachedFractionalRemainerFractionOfTotal = (float)yPatternElements.Segments[i].Value / (float)yFlexTotalWeight;
					yPatternElements.Segments[i].CachedFractionalRemainderMagnitude = yFlexElementMagnitudeTotal * yPatternElements.Segments[i].CachedFractionalRemainerFractionOfTotal;
				}
			}
		}


		if (FMath::IsNearlyZero(yAxisLength, KINDA_SMALL_NUMBER) || yFlexTotalWeight > 0)
			yAxisLength = yDimension;
		if (FMath::IsNearlyZero(xAxisLength, KINDA_SMALL_NUMBER) || xFlexTotalWeight > 0)
			xAxisLength = xDimension;

		//calculate how many iterations of the pattern there will be for each axis
		if (FMath::IsNearlyZero(xAxisLength, KINDA_SMALL_NUMBER) || FMath::IsNearlyZero(yAxisLength, KINDA_SMALL_NUMBER))
		{
			return;
		}

		float xIterations = xDimension / xAxisLength;
		float yIterations = yDimension / yAxisLength;
		

		TArray<TArray<FVector>> planesAndPoints;
		TArray<FVector> planePoints;

		FVector2D point1, point2, point3, point4;
		float xOffsetAccumulator = 0.0f;
		float yOffsetAccumulator = 0.0f;
		float xOffsetAccumulatorDynamic = 0.0f;
		float yOffsetAccumulatorDynamic = 0.0f;
		int32 graphIDCounter = 1;
		int32 outID = 0;
		TSet<int32> groupIDs;
		FGraph3DDelta graphDelta;
		bool bEssentialElement = true;
		for (int gridYIndex = 0; gridYIndex < (FMath::CeilToInt(yIterations) * yPatternElements.Segments.Num()); gridYIndex++)
		{
			//determine the proper value to use for this pass depending on if it's a fractional remainder segment
			float yValue;
			if (yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].SegmentType == EBIMPatternSegment::FractionalRemainder)
			{
				yValue = yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].CachedFractionalRemainderMagnitude;
			}
			else {
				yValue = yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].Value;
			}
			for (int gridXIndex = 0; gridXIndex < (FMath::CeilToInt(xIterations) * xPatternElements.Segments.Num()); gridXIndex++)
			{
				//if we're past the first full iteration of the pattern this is no longer part of the essential element
				if ((gridXIndex > xPatternElements.Segments.Num()) || (gridYIndex > xPatternElements.Segments.Num()))
				{
					bEssentialElement = false;
				}
				//determine the proper value to use for this pass depending on if it's a fractional remainder segment
				float xValue;
				if (xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].SegmentType == EBIMPatternSegment::FractionalRemainder)
				{
					xValue = xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].CachedFractionalRemainderMagnitude;
				}
				else {
					xValue = xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].Value;
				}
				
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

				//need to lerp the points into the plane's space
				point1.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point1.X / xDimension));
				point1.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point1.Y / yDimension));

				point2.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point2.X / xDimension));
				point2.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point2.Y / yDimension));

				point3.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point3.X / xDimension));
				point3.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point3.Y / yDimension));

				point4.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point4.X / xDimension));
				point4.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point4.Y / yDimension));
				
				//create an array of the deprojected points
				FOccluderVertexArray facePoints;
				facePoints.Add(parentFace->DeprojectPosition(point1));
				facePoints.Add(parentFace->DeprojectPosition(point2));
				facePoints.Add(parentFace->DeprojectPosition(point3));
				facePoints.Add(parentFace->DeprojectPosition(point4));

				//generate essential element
				if (bEssentialElement)
				{
					//accumulate deltas for vertices
					int32 vertID1, vertID2, vertID3, vertID4;

					CachedEssentialElementGraph->GetDeltaForVertexAddition(facePoints[0], graphDelta, graphIDCounter, vertID1);
					CachedEssentialElementGraph->GetDeltaForVertexAddition(facePoints[1], graphDelta, graphIDCounter, vertID2);
					CachedEssentialElementGraph->GetDeltaForVertexAddition(facePoints[2], graphDelta, graphIDCounter, vertID3);
					CachedEssentialElementGraph->GetDeltaForVertexAddition(facePoints[3], graphDelta, graphIDCounter, vertID4);

					//accumulate deltas for edge additions
					TArray<int32> outIDs;
					int32 existingID;
					CachedEssentialElementGraph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID1, vertID2), graphDelta, graphIDCounter, existingID, outIDs);
					outIDs.Reset();
					CachedEssentialElementGraph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID2, vertID3), graphDelta, graphIDCounter, existingID, outIDs);
					outIDs.Reset();
					CachedEssentialElementGraph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID3, vertID4), graphDelta, graphIDCounter, existingID, outIDs);
					outIDs.Reset();
					CachedEssentialElementGraph->GetDeltaForMultipleEdgeAdditions(FGraphVertexPair(vertID4, vertID1), graphDelta, graphIDCounter, existingID, outIDs);
					outIDs.Reset();

					//accumulate face addition delta
					TArray<int32> vertIDs;
					vertIDs.Add(vertID1);
					vertIDs.Add(vertID2);
					vertIDs.Add(vertID3);
					vertIDs.Add(vertID4);
					TMap<int32, int32> parentEdgeIdxToId;
					TArray<int32> parentFaceIDs;
					CachedEssentialElementGraph->GetDeltaForFaceAddition(vertIDs, graphDelta, graphIDCounter, existingID, parentFaceIDs, parentEdgeIdxToId, outID);
				}
				

				//NOTE: in the following code "dynamic" means the magnitudes are being calculated based on the actual face
				if (bCalculateDynamic)
				{
					bool bIntersectionFound = false;
					float dist;
					float dynamicFRMagnitude; //dynamically calculated fractional remainder magnitude
					float dynamicXOffset = 0.0f, dynamicYOffset = 0.0f;
					FVector2D intersectingEdge1, intersectingEdge2;
					//find x for point1 -> point2
					bIntersectionFound = GetDistanceBetweenIntersectingEdgesOfParentGraph(point1, point2, intersectingEdge1, intersectingEdge2);
					dist = (intersectingEdge1 - intersectingEdge2).Size();
					dynamicFRMagnitude = dist - xAxisLength;
					if ((bIntersectionFound))
					{
						if (xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].SegmentType == EBIMPatternSegment::FractionalRemainder)
						{
							dynamicXOffset = dynamicFRMagnitude * xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].CachedFractionalRemainerFractionOfTotal;
						}
						else {
							dynamicXOffset = xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].Value;
						}
						point1.X = xOffsetAccumulatorDynamic;
						point2.X = xOffsetAccumulatorDynamic + dynamicXOffset;
					}
					bIntersectionFound = GetDistanceBetweenIntersectingEdgesOfParentGraph(point3, point4, intersectingEdge1, intersectingEdge2);
					dist = (intersectingEdge1 - intersectingEdge2).Size();
					dynamicFRMagnitude = dist - xAxisLength;
					if ((bIntersectionFound))
					{
						if (xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].SegmentType == EBIMPatternSegment::FractionalRemainder)
						{
							dynamicXOffset = dynamicFRMagnitude * xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].CachedFractionalRemainerFractionOfTotal;
						}
						else {
							dynamicXOffset = xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].Value;
						}
						point3.X = xOffsetAccumulatorDynamic + dynamicXOffset;
						point4.X = xOffsetAccumulatorDynamic;
					}


					if (yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].SegmentType == EBIMPatternSegment::FractionalRemainder)
					{
						bIntersectionFound = GetDistanceBetweenIntersectingEdgesOfParentGraph(point1, point4, intersectingEdge1, intersectingEdge2);
						dist = (intersectingEdge1 - intersectingEdge2).Size();
						if ((bIntersectionFound))
						{
							point1.Y = intersectingEdge2.Y;
							point4.Y = intersectingEdge1.Y;
						}
						bIntersectionFound = GetDistanceBetweenIntersectingEdgesOfParentGraph(point2, point3, intersectingEdge1, intersectingEdge2);
						dist = (intersectingEdge1 - intersectingEdge2).Size();
						if ((bIntersectionFound))
						{
							point2.Y = intersectingEdge2.Y;
							point3.Y = intersectingEdge1.Y;
						}
					}
					xOffsetAccumulatorDynamic += dynamicXOffset;
					facePoints.Reset();
					facePoints.Add(parentFace->DeprojectPosition(point1));
					facePoints.Add(parentFace->DeprojectPosition(point2));
					facePoints.Add(parentFace->DeprojectPosition(point3));
					facePoints.Add(parentFace->DeprojectPosition(point4));
				}
				
				//for displaying the debug dynamic mesh
				planePoints.Add(facePoints[0]);
				planePoints.Add(facePoints[1]);
				planePoints.Add(facePoints[2]);
				planePoints.Add(facePoints[3]);
				planesAndPoints.Add(planePoints);
				
			}
			xOffsetAccumulator = 0.0f;
			yOffsetAccumulator += yValue;
		}

		//apply all the deltas for the essential element
		CachedEssentialElementGraph->ApplyDelta(graphDelta);

		FPattern2DParams patternParams;
		patternParams.MeshsToCreate = planesAndPoints;

		//calculate slicers based on parent face geometry
		for (int edgeIndex = 0; edgeIndex < parentFace->EdgeIDs.Num(); edgeIndex++)
		{
			const FGraph3DEdge* edge = Document->GetVolumeGraph()->FindEdge(parentFace->EdgeIDs[edgeIndex]);
			FSlicer slicerToAdd;
			slicerToAdd.SliceOrigin = edge->CachedMidpoint;
			slicerToAdd.SliceNormal = parentFace->CachedEdgeNormals[edgeIndex];
			patternParams.Slicers.Add(slicerToAdd);
		}

		DynamicMeshActor->SetupPattern2DGeometry(patternParams);
		
	}
}
