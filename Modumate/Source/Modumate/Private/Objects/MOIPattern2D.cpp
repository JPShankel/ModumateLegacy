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

	FPattern2DParams patternParams;
	//define a hardcoded pattern
	FPatternAxisData x1, x2, xFlex;
	x1.bFlex = false;
	x1.Magnitude = 100.0f;
	xFlex.bFlex = true;
	x2.bFlex = false;
	x2.Magnitude = 200.0f;
	patternParams.XAxisPattern.Add(x1);
	patternParams.XAxisPattern.Add(xFlex);
	patternParams.XAxisPattern.Add(x2);
	patternParams.XAxisPattern.Add(x1);
	patternParams.YAxisPattern.Add(xFlex);
	patternParams.YAxisPattern.Add(x1);
	// TODO: initial visual is just the hosted plane, future work: schematic layout of pattern
	const AMOIMetaPlane* parent = Cast<AMOIMetaPlane>(GetParentObject());
	if (parent != nullptr)
	{

		const FGraph3DFace *parentFace = Document->GetVolumeGraph()->FindFace(GetParentObject()->ID);

		FArchitecturalMaterial material1, material2;

		auto* gameInstance = GetWorld()->GetGameInstance<UModumateGameInstance>();
		material1.EngineMaterial = gameInstance->GetEditModelGameMode()->GreenMaterial;
		material2.EngineMaterial = gameInstance->GetEditModelGameMode()->GreenMaterial;
		material1.Color = FColor::White;
		material2.Color = FColor::Red;
		float xDimension, yDimension;

		//making a lot of assumptions here to get the dimensions from the points, can probably get the dimensions from the meta plane.
		FBox2D boundsBox2D = FBox2D(parentFace->Cached2DPositions);
		xDimension = boundsBox2D.Max.X - boundsBox2D.Min.X;
		yDimension = boundsBox2D.Max.Y - boundsBox2D.Min.Y;
		
		
		float xAxisLength = 0.0f;
		float yAxisLength = 0.0f;
		int32 xFlexCount = 0;
		int32 yFlexCount = 0;
		TArray<FPatternAxisData> xPatternElements, yPatternElements;
		xPatternElements = patternParams.XAxisPattern;
		yPatternElements = patternParams.YAxisPattern;
		//calculate essential element x length
		for (FPatternAxisData& patternAxisDataX : patternParams.XAxisPattern)
		{
			//if any regions are flex, the length becomes the plane length
			if (patternAxisDataX.bFlex)
			{
				xFlexCount++;
			}
			else {
				xAxisLength += patternAxisDataX.Magnitude;
			}
		}
		//calculate essential element y length
		for (FPatternAxisData& patternAxisDataY : patternParams.YAxisPattern)
		{
			//if any regions are flex, the length becomes the plane length
			if (patternAxisDataY.bFlex)
			{
				yFlexCount++;
			}
			else {
				yAxisLength += patternAxisDataY.Magnitude;
			}
		}

		//Calculate flex element magnitudes and update magnitudes in pattern definition
		float xFlexElementMagnitude = 0.0f;
		float yFlexElementMagnitude = 0.0f;
		if (xFlexCount > 0)
		{
			xFlexElementMagnitude = (xDimension - xAxisLength) / xFlexCount;
			for (int32 i = 0; i < xPatternElements.Num(); i++)
			{
				if (xPatternElements[i].bFlex)
				{
					xPatternElements[i].Magnitude = xFlexElementMagnitude;
				}
			}
		}
		if (yFlexCount > 0)
		{
			yFlexElementMagnitude = (yDimension - yAxisLength) / yFlexCount;
			for (int32 i = 0; i < yPatternElements.Num(); i++)
			{
				if (yPatternElements[i].bFlex)
				{
					yPatternElements[i].Magnitude = yFlexElementMagnitude;
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

		FVector2D point1, point2, point3, point4 = FVector2D::ZeroVector;
		float xOffsetAccumulator = 0.0f;
		float yOffsetAccumulator = 0.0f;
		for (int gridYIndex = 0; gridYIndex < (FMath::CeilToInt(yIterations) * yPatternElements.Num()); gridYIndex++)
		{
			for (int gridXIndex = 0; gridXIndex < (FMath::CeilToInt(xIterations) * xPatternElements.Num()); gridXIndex++)
			{
				//if this face would go over the bounds of the plane handle it differently, for now cull
				if (((xOffsetAccumulator + xPatternElements[gridXIndex % xPatternElements.Num()].Magnitude) > xDimension) || ((yOffsetAccumulator + yPatternElements[gridYIndex % yPatternElements.Num()].Magnitude) > yDimension))
				{
					break;
				}
				point1 = point2 = point3 = point4 = FVector2D::ZeroVector;
				planePoints.Empty();

				point1.X = xOffsetAccumulator;
				point1.Y = yOffsetAccumulator;

				point2.X = xPatternElements[gridXIndex % xPatternElements.Num()].Magnitude + xOffsetAccumulator;
				point2.Y = yOffsetAccumulator;

				point3.X = xPatternElements[gridXIndex % xPatternElements.Num()].Magnitude + xOffsetAccumulator;
				point3.Y = yPatternElements[gridYIndex % yPatternElements.Num()].Magnitude + yOffsetAccumulator;

				point4.X = xOffsetAccumulator;
				point4.Y = yPatternElements[gridYIndex % yPatternElements.Num()].Magnitude + yOffsetAccumulator;

				xOffsetAccumulator += xPatternElements[gridXIndex % xPatternElements.Num()].Magnitude;

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
				//
			}
			xOffsetAccumulator = 0.0f;
			yOffsetAccumulator += yPatternElements[gridYIndex % yPatternElements.Num()].Magnitude;
		}
		patternParams.MeshsToCreate = planesAndPoints;
		DynamicMeshActor->SetupPattern2DGeometry(patternParams);
	}
}
