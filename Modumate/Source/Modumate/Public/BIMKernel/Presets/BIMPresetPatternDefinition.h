// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMPresetPatternDefinition.generated.h"

/*
* Sequences of pattern segments laid along the 3 cardinal directions define the vertices of an irregular grid
* The grid is used to create metagraph vertices, edges and faces which will ultimately host physical MOIs
*/

UENUM()
enum class EBIMPatternSegment : uint8
{
	None=0,
	Absolute,
	PercentageOfLargestChild,
	FractionalRemainder
};

USTRUCT()
struct MODUMATE_API FBIMPatternSegment
{
	GENERATED_BODY()

	UPROPERTY()
	EBIMPatternSegment SegmentType = EBIMPatternSegment::None;

	UPROPERTY()
	float Value = 0.0f;

	//this is the worst case fractional remainder magnitude, meaning it's based on the longest bound of the parent plane
	float CachedFractionalRemainderMagnitude = 0.0f;

	float CachedFractionalRemainerFractionOfTotal = 0.0f;
};

USTRUCT()
struct MODUMATE_API FBIMPatternSequence
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBIMPatternSegment> Segments;

	
};

/*
* Given a set of sequences defining the grid, we can reference elements (verts, edges or faces) with integer coordinates
* For verts, the coordinate specifies which segment in each dimension defines the point
* For edges, we have 3 directions which point along the positive axes from a specified vert
* For faces, we specify the minimum x,y vert corner of the face
* For volumes, we specify the minimum x,y,z vert corner of the volume
* 
* These coordinate/element indicators are used to create spans within the pattern
*/
USTRUCT()
struct MODUMATE_API FBIMPatternCoordinate
{
	GENERATED_BODY()

	UPROPERTY()
	int32 X = 0;

	UPROPERTY()
	int32 Y = 0;

	UPROPERTY()
	int32 Z = 0;

	void FromCSVCells(const TArray<FString>& Cells);
};

UENUM()
enum class EBIMSpanElement
{
	None = 0,
	ParentFaceNormal,
	Vertex,
	EdgeX,
	EdgeY,
	EdgeZ,
	FaceXY,
	FaceXZ,
	FaceYZ,
	Volume
};

USTRUCT()
struct MODUMATE_API FBIMSpanElement
{
	GENERATED_BODY()

	UPROPERTY()
	EBIMSpanElement ElementType = EBIMSpanElement::None;

	UPROPERTY()
	FBIMPatternCoordinate Coordinate;
};

USTRUCT()
struct MODUMATE_API FBIMSpanBasis
{
	GENERATED_BODY()

	UPROPERTY()
	FBIMSpanElement Element;
};

/*
* Spans have a single element type and host a single MOI
*/
USTRUCT()
struct MODUMATE_API FBIMSpan
{
	GENERATED_BODY()

	UPROPERTY()
	EBIMSpanElement ElementType = EBIMSpanElement::None;

	UPROPERTY()
	TArray<FBIMPatternCoordinate> Elements;

	UPROPERTY()
	TArray<FBIMSpanElement> Cutters;

	UPROPERTY()
	FGuid HostedPreset;

	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector Flip= FVector::ZeroVector;

	UPROPERTY()
	FString ID;

	UPROPERTY()
	FBIMPatternCoordinate Origin;

	UPROPERTY()
	FBIMSpanBasis BasisX;

	UPROPERTY()
	FBIMSpanBasis BasisZ;
};

/*
* FBIMPattern is the top-level custom data present in the preset
* It defines sequences along each cardinal line and a set of spans to host objects
*/
USTRUCT()
struct MODUMATE_API FBIMPattern
{
	GENERATED_BODY()

	UPROPERTY()
	FBIMPatternSequence SequenceX;

	UPROPERTY()
	FBIMPatternSequence SequenceY;

	UPROPERTY()
	FBIMPatternSequence SequenceZ;

	UPROPERTY()
	TArray<FBIMSpan> Spans;
};
