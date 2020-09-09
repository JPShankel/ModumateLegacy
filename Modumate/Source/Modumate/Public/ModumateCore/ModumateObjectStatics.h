// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Graph/Graph3DTypes.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UObject/Object.h"

#include "ModumateObjectStatics.generated.h"

class FBIMAssemblySpec;
class FModumateDocument;
class FModumateObjectInstance;
struct FSimplePolygon;

namespace Modumate
{
	class FGraph3DEdge;
};


UENUM(BlueprintType)
enum class ETrimMiterOptions : uint8
{
	None,
	MiterAngled,
	RunPast,
	RunUntil,
};

// Helper functions for creating / updating MOIs and their geometry.
// When multiple systems need to understand the relationship between control points/indices and MOI geometry,
// such as Tools, procedural meshes, icon generation, MOI interface implementations, etc., those functions can go here.
UCLASS(BlueprintType)
class MODUMATE_API UModumateObjectStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Trims
	static ETrimMiterOptions GetMiterOptionFromAdjacentTrim(ETrimMiterOptions OtherMiterOption);

	static bool GetPolygonProfile(const FBIMAssemblySpec *TrimAssembly, const FSimplePolygon*& OutProfile);

	static bool GetTrimValuesFromControls(const TArray<FVector> &ControlPoints, const TArray<int32> &ControlIndices,
		float &OutTrimStartLength, float &OutTrimEndLength, int32 &OutEdgeStartIndex, int32 &OutEdgeEndIndex,
		int32 &OutMountIndex, bool &bOutUseLengthAsPercent, ETrimMiterOptions &OutMiterOptionStart, ETrimMiterOptions &OutMiterOptionEnd);

	static bool GetTrimControlsFromValues(float TrimStartLength, float TrimEndLength,
		int32 EdgeStartIndex, int32 EdgeEndIndex, int32 MountIndex,
		bool bUseLengthAsPercent, ETrimMiterOptions MiterOptionStart, ETrimMiterOptions MiterOptionEnd,
		TArray<FVector> &OutControlPoints, TArray<int32> &OutControlIndices);

	static bool GetTrimEdgePosition(const FModumateObjectInstance* TargetObject,
		int32 EdgeStartIndex, int32 EdgeEndIndex, FVector &OutEdgeStart, FVector &OutEdgeEnd);

	static bool GetTrimGeometryOnEdge(const FModumateObjectInstance* TargetObject,
		const FBIMAssemblySpec *TrimAssembly, int32 EdgeStartIndex, int32 EdgeEndIndex,
		float TrimStartLength, float TrimEndLength, bool bUseLengthAsPercent,
		ETrimMiterOptions TrimMiterStart, ETrimMiterOptions TrimMiterEnd,
		FVector &OutTrimStart, FVector &OutTrimEnd, FVector &OutTrimNormal, FVector &OutTrimUp, int32 &OutMountIndex,
		FVector2D &OutTrimUpperExtensions, FVector2D &OutTrimOuterExtensions,
		const FVector &HintNormal = FVector::ZeroVector, int32 HintMountIndex = -1, bool bDoMitering = false);

	static bool GetRelativeTransformOnPlanarObj(
		const FModumateObjectInstance *PlanarObj,
		const FVector &WorldPos, float DistanceFromBottom,
		bool bUseDistanceFromBottom, FVector2D &OutRelativePos,
		FQuat &OutRelativeRot);

	static bool GetWorldTransformOnPlanarObj(
		const FModumateObjectInstance *PlanarObj,
		const FVector2D &RelativePos, const FQuat &RelativeRot,
		FVector &OutWorldPos, FQuat &OutWorldRot);

	// Face-mounted objects
	static int32 GetParentFaceIndex(const FModumateObjectInstance *FaceMountedObj);
	static int32 GetFaceIndexFromTargetHit(const FModumateObjectInstance *HitObject, const FVector &HitLocation, const FVector &HitNormal);
	static bool GetGeometryFromFaceIndex(const FModumateObjectInstance *Host, int32 FaceIndex,
		TArray<FVector>& OutFacePoints, FVector& OutNormal, FVector& OutFaceAxisX, FVector& OutFaceAxisY);
	static bool GetGeometryFromFaceIndex(const FModumateObjectInstance *Host, int32 FaceIndex,
		TArray<FVector>& OutFacePoints, FTransform& OutFaceOrigin);
	static bool GetGeometryFromSurfacePoly(const FModumateDocument* Doc, int32 SurfacePolyID, bool& bOutInterior, bool& bOutInnerBounds,
		FTransform& OutOrigin, TArray<FVector>& OutPerimeter, TArray<FPolyHole3D>& OutHoles);

	// Meta Objects
	static void EdgeConnectedToValidPlane(const Modumate::FGraph3DEdge *GraphEdge, const FModumateDocument *Doc,
		bool &bOutConnectedToEmptyPlane, bool &bOutConnectedToSelectedPlane);
	static void ShouldMetaObjBeEnabled(const FModumateObjectInstance *MetaMOI,
		bool &bOutShouldBeVisible, bool &bOutShouldCollisionBeEnabled, bool &bOutIsConnected);

	static void GetGraphIDsFromMOIs(const TArray<FModumateObjectInstance *> &MOIs, TSet<int32> &OutGraphObjIDs);

	// Given a plane hosted object, find some basic values:
	// the thickness of the assembly,
	// the distance along its normal vector from which the rest of its layers are positioned,
	// and the plane normal.
	static void GetPlaneHostedValues(const FModumateObjectInstance *PlaneHostedObj, float &OutThickness, float &OutStartOffset, FVector &OutNormal);

	// Given a plane hosted object, find the outermost delta vectors from the plane to its layer extents.
	// This does not include finishes or other hosted children (yet).
	static void GetExtrusionDeltas(const FModumateObjectInstance *PlaneHostedObj, FVector &OutStartDelta, FVector &OutEndDelta);

	// FF&E
	static bool GetMountedTransform(const FVector &MountOrigin, const FVector &MountNormal,
		const FVector &LocalDesiredNormal, const FVector &LocalDesiredTangent, FTransform &OutTransform);

	static bool GetFFEMountedTransform(const FBIMAssemblySpec *Assembly,
		const FSnappedCursor &SnappedCursor, FTransform &OutTransform);

	static bool GetFFEBoxSidePoints(AActor *Actor, const FVector &AssemblyNormal, TArray<FVector> &OutPoints);
};
