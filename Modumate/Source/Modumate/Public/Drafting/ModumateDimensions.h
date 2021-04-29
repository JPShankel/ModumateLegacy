// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateTypes.h"
#include "VectorTypes.h"

class UModumateDocument;
namespace Modumate
{
	class FGraph2D;
}

namespace Modumate
{
	class FDraftingComposite;
}

class FModumateDimension
{
public:
	FModumateDimension() = default;
	FModumateDimension(FVec2d StartVec, FVec2d EndVec);
	explicit operator bool() const { return bActive; }

	FVec2d Points[2];  // World units, ie. cm.
	FVec2d TextPosition;
	FVec2d Dir;
	double Length = 0.0;
	int32 Graph2DID[2] = { MOD_ID_NONE, MOD_ID_NONE };
	int32 MetaplaneID = 0;
	int32 Depth = INT_MAX;  // Graph hops from perimeter, filled in by breadth-first search.
	bool bActive = false;
	bool bHorizontal = false;
	bool bVertical = false;
	bool bPortal = false;
	enum EType { Opening, Framing, Massing, Bridging, Reference };
	enum ESide { Left, Right };
	EType DimensionType = Framing;
	ESide LineSide = Left;
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
	bool AddDimensionsFromCutPlane(TSharedPtr<Modumate::FDraftingComposite>& Page, const UModumateDocument * Doc,
		FPlane Plane, FVector Origin, FVector AxisX);

private:
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

	TSharedPtr<Modumate::FGraph2D> CutGraph;
	TMap<int32, int32> GraphIDToObjID;
	TMap<int32, int32> GraphIDToDimID;
	TArray<FModumateDimension> Dimensions;
	TArray<FModumateAngularDimension> AngularDimensions;

	static constexpr double OpeningDimOffset = 56.0;
	static constexpr double FramingDimOffset = 76.0;
	static constexpr double MassingDimOffset = 96.0;
	static constexpr double BridgingDimOffset = 30.0;
};
