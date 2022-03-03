// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/MOIPattern2D.h"
#include "Objects/MetaPlane.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "UnrealClasses/EditModelGameMode.h"
AMOIPattern2D::AMOIPattern2D()
{}

void AMOIPattern2D::SetupDynamicGeometry()
{
	UpdateDynamicGeometry();
}

void AMOIPattern2D::UpdateDynamicGeometry()
{
	Super::SetupDynamicGeometry();

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
		int32 xFlexCount = 0;
		int32 yFlexCount = 0;
		FBIMPatternSequence xPatternElements, yPatternElements;
		xPatternElements = CachedAssembly.PatternData.SequenceX;
		yPatternElements = CachedAssembly.PatternData.SequenceY;
		//calculate essential element x length
		for (FBIMPatternSegment& patternAxisDataX : xPatternElements.Segments)
		{
			//if any regions are flex, the length becomes the plane length
			if (patternAxisDataX.SegmentType == EBIMPatternSegment::FractionalRemainder)
			{
				xFlexCount += patternAxisDataX.Value;
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
				yFlexCount += patternAxisDataY.Value;
			}
			else {
				yAxisLength += patternAxisDataY.Value;
			}
		}

		//Calculate flex element magnitudes and update magnitudes in pattern definition
		float xFlexElementMagnitudeTotal = 0.0f;
		float yFlexElementMagnitudeTotal = 0.0f;
		if (xFlexCount > 0)
		{
			xFlexElementMagnitudeTotal = (xDimension - xAxisLength);
			for (int32 i = 0; i < xPatternElements.Segments.Num(); i++)
			{
				if (xPatternElements.Segments[i].SegmentType == EBIMPatternSegment::FractionalRemainder)
				{
					xPatternElements.Segments[i].Value = xFlexElementMagnitudeTotal * ((float)xPatternElements.Segments[i].Value / (float)xFlexCount);
				}
			}
		}
		if (yFlexCount > 0)
		{
			yFlexElementMagnitudeTotal = (yDimension - yAxisLength);
			for (int32 i = 0; i < yPatternElements.Segments.Num(); i++)
			{
				if (yPatternElements.Segments[i].SegmentType == EBIMPatternSegment::FractionalRemainder)
				{
					yPatternElements.Segments[i].Value = yFlexElementMagnitudeTotal * ((float)yPatternElements.Segments[i].Value / (float)yFlexCount);
				}
			}
		}


		if (FMath::IsNearlyZero(yAxisLength, KINDA_SMALL_NUMBER) || yFlexCount > 0)
			yAxisLength = yDimension;
		if (FMath::IsNearlyZero(xAxisLength, KINDA_SMALL_NUMBER) || xFlexCount > 0)
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
		for (int gridYIndex = 0; gridYIndex < (FMath::CeilToInt(yIterations) * yPatternElements.Segments.Num()); gridYIndex++)
		{
			for (int gridXIndex = 0; gridXIndex < (FMath::CeilToInt(xIterations) * xPatternElements.Segments.Num()); gridXIndex++)
			{
				//if this face would go over the bounds of the plane handle it differently, for now cull
				if (((xOffsetAccumulator + xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].Value) > xDimension) || ((yOffsetAccumulator + yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].Value) > yDimension))
				{
					break;
				}
				point1 = point2 = point3 = point4 = FVector2D::ZeroVector;
				planePoints.Empty();

				point1.X = xOffsetAccumulator;
				point1.Y = yOffsetAccumulator;

				point2.X = xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].Value + xOffsetAccumulator;
				point2.Y = yOffsetAccumulator;

				point3.X = xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].Value + xOffsetAccumulator;
				point3.Y = yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].Value + yOffsetAccumulator;

				point4.X = xOffsetAccumulator;
				point4.Y = yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].Value + yOffsetAccumulator;

				xOffsetAccumulator += xPatternElements.Segments[gridXIndex % xPatternElements.Segments.Num()].Value;

				//need to lerp the points into the plane's space
				point1.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point1.X / xDimension));
				point1.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point1.Y / yDimension));

				point2.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point2.X / xDimension));
				point2.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point2.Y / yDimension));

				point3.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point3.X / xDimension));
				point3.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point3.Y / yDimension));

				point4.X = FMath::Lerp(boundsBox2D.Min.X, boundsBox2D.Max.X, (point4.X / xDimension));
				point4.Y = FMath::Lerp(boundsBox2D.Min.Y, boundsBox2D.Max.Y, (point4.Y / yDimension));
				
				planePoints.Add(parentFace->DeprojectPosition(point1));
				planePoints.Add(parentFace->DeprojectPosition(point2));
				planePoints.Add(parentFace->DeprojectPosition(point3));
				planePoints.Add(parentFace->DeprojectPosition(point4));
				planesAndPoints.Add(planePoints);
			}
			xOffsetAccumulator = 0.0f;
			yOffsetAccumulator += yPatternElements.Segments[gridYIndex % yPatternElements.Segments.Num()].Value;
		}
		FPattern2DParams patternParams;
		patternParams.MeshsToCreate = planesAndPoints;
		DynamicMeshActor->SetupPattern2DGeometry(patternParams);
	}
}
