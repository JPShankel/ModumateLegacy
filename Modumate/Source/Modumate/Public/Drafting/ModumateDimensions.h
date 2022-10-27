// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "VectorTypes.h"

#include "Objects/ModumateObjectEnums.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph2DEdge.h"
#include "Graph/Graph2DVertex.h"

#include "ModumateDimensions.generated.h"

class UModumateDocument;
class FGraph2D;

UENUM()
enum class EDimensionType { Opening, Framing, Massing, Bridging, Reference };
	
UENUM()
enum class EDimensionSide { Left, Right };

USTRUCT()
struct MODUMATE_API FModumateDimension
{
	GENERATED_BODY()

	FModumateDimension() = default;
	FModumateDimension(FVec2d StartVec, FVec2d EndVec);
	explicit operator bool() const { return bActive; }

	UPROPERTY()
	FVec2d Points[2];  // World units, ie. cm.
	
	UPROPERTY()
	FVec2d TextPosition;

	UPROPERTY()
	FVec2d Dir;

	UPROPERTY()
	double Length = 0.0;

	UPROPERTY()
	int32 Graph2DID[2] = { MOD_ID_NONE, MOD_ID_NONE };

	UPROPERTY()
	int32 MetaplaneID = 0;

	UPROPERTY()
	int32 Depth = INT_MAX;  // Graph hops from perimeter, filled in by breadth-first search.

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	bool bHorizontal = false;

	UPROPERTY()
	bool bVertical = false;

	UPROPERTY()
	bool bPortal = false;

	UPROPERTY()
	EDimensionType DimensionType = EDimensionType::Framing;

	UPROPERTY()
	EDimensionSide LineSide = EDimensionSide::Left;
	
	FVector2<bool> StartFixed = { false, false };
	FVector2<bool> EndFixed = { false, false };
	TArray<int32> Connections[2];  // start, end connected dimensions, by index number.
};

class FModumateAngularDimension
{
public:
	FModumateAngularDimension() = default;
	FModumateAngularDimension(FVec2d StartPosition, FVec2d EndPosition, FVec2d CenterPosition)
		: Start(StartPosition), End(EndPosition), Center(CenterPosition) { }

	FVec2d Start;
	FVec2d End;
	FVec2d Center;
};

class FModumateDimensions
{
public:
	bool AddDimensionsFromCutPlane(TSharedPtr<FDraftingComposite>& Page, const UModumateDocument * Doc,
	                               FPlane Plane, FVector Origin, FVector AxisX, const TSet<int32>* Groups = nullptr);
	TArray<FModumateDimension> GetDimensions(const UModumateDocument * Doc,
							FPlane Plane, FVector Origin, FVector AxisX);

private:

	bool PopulateAndProcessDimensions(const UModumateDocument * Doc,
							FPlane Plane, FVector Origin, FVector AxisX, const TSet<int32>* Groups);
	void Reset();
	void ProcessDimensions();
	void AddEdgeAndConnected(int32 Edge, TSet<int32>& OutEdges) const;
	void ProcessConnectedGroup(const TSet<int32>& Group);
	int32 SmallestConnectedEdgeAngle(const FVec2d& Point, const FVec2d& Dir, const TArray<int32>& Connected) const;
	void PropagateStatus(int32 d, int32 vert);
	void PropagateHorizontalStatus(int32 d, int32 vert);
	void PropagateVerticalStatus(int32 d, int32 vert);
	void DropLongestOpeningDimension(const TArray<int32>* OpeningIds);
	void AddAngularDimensions(const TArray<int32>& Group);
	void CreateAngularDimension(int32 Edge1, int32 Vertex, int32 Edge2);
	void ConnectIslands(const TArray<TSet<int32>>& plans);
	FVec2d FarPoint(int32 DimensionIndex, int32 VertexIndex, int32 ConnectionIndex) const;
	static float GetEdgeAngle(const FGraph2DEdge* Edge, bool bFlippedEdge = false);
	
	TMap<int32, int32> GraphIDToObjID;
	TMap<int32, int32> GraphIDToDimID;
	TMap<int32, FGraph2DEdge> Edges;
	TMap<int32, FGraph2DVertex> Vertices;

	TArray<FModumateDimension> Dimensions;
	TArray<FModumateAngularDimension> AngularDimensions;
	
	static constexpr double OpeningDimOffset = 56.0;
	static constexpr double FramingDimOffset = 76.0;
	static constexpr double MassingDimOffset = 96.0;
	static constexpr double BridgingDimOffset = 30.0;
};
