// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "ModumateCore/ModumateGeometryStatics.h"

#include "LayerGeomDef.generated.h"

// Define a struct that can fully define the geometry necessary to triangulate a layer in a plane-hosted object, or generally any prism.
USTRUCT()
struct MODUMATE_API FLayerGeomDef
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FVector> PointsA;

	UPROPERTY()
	TArray<FVector> PointsB;

	UPROPERTY()
	TArray<FPolyHole3D> Holes3D;

	UPROPERTY()
	float Thickness = 0.0f;

	UPROPERTY()
	FVector Normal = FVector::ZeroVector;

	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	UPROPERTY()
	FVector AxisX = FVector::ZeroVector;

	UPROPERTY()
	FVector AxisY = FVector::ZeroVector;

	UPROPERTY()
	bool bValid = false;

	UPROPERTY()
	bool bCoincident = false;

	UPROPERTY()
	TArray<FVector2D> CachedPointsA2D;

	UPROPERTY()
	TArray<FVector2D> CachedPointsB2D;

	UPROPERTY()
	TArray<FPolyHole2D> CachedHoles2D;

	UPROPERTY()
	TArray<FPolyHole2D> ValidHoles2D;

	UPROPERTY()
	TArray<bool> HolesValid;

	mutable TArray<FVector> TempSidePoints;

	FLayerGeomDef();

	// A simplified constructor, for non-mitered, hole-less layers
	FLayerGeomDef(const TArray<FVector>& Points, float InThickness, const FVector& InNormal);

	FLayerGeomDef(const TArray<FVector>& InPointsA, const TArray<FVector>& InPointsB, const FVector& InNormal,
		const FVector& InAxisX = FVector::ZeroVector, const TArray<FPolyHole3D> *InHoles = nullptr);

	void Init(const TArray<FVector>& InPointsA, const TArray<FVector>& InPointsB, const FVector& InNormal,
		const FVector& InAxisX = FVector::ZeroVector, const TArray<FPolyHole3D> *InHoles = nullptr);
	FVector2D ProjectPoint2D(const FVector& Point3D) const;
	FVector2D ProjectPoint2DSnapped(const FVector& Point3D, float Tolerance = KINDA_SMALL_NUMBER) const;
	FVector Deproject2DPoint(const FVector2D& Point2D, bool bSideA) const;
	FVector ProjectToPlane(const FVector& Point, bool bSideA) const;
	bool CachePoints2D();

	static void AppendTriangles(const TArray<FVector>& Verts, const TArray<int32>& SourceTriIndices,
		TArray<int32>& OutTris, bool bReverseTris);
	bool TriangulateSideFace(const FVector2D& Point2DA1, const FVector2D& Point2DA2,
		const FVector2D& Point2DB1, const FVector2D& Point2DB2, bool bReverseTris,
		TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FProcMeshTangent>& OutTangents,
		const FVector2D& UVScale, const FVector& UVAnchor = FVector::ZeroVector, float UVRotOffset = 0.0f) const;
	bool TriangulateMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FProcMeshTangent>& OutTangents,
		const FVector& UVAnchor = FVector::ZeroVector, float UVRotOffset = 0.0f) const;

	// Divide Intersection by any projected layer-holes; return as parametric ranges.
	void GetRangesForHolesOnPlane(TArray<TPair<float, float>>& OutRanges, TPair<FVector, FVector>& Intersection, const FVector& HoleOffset, const FPlane& Plane, const FVector& AxisX, const FVector& AxisY, const FVector& Origin) const;
};
