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
class UModumateDocument;
class AModumateObjectInstance;
struct FSimplePolygon;

namespace Modumate
{
	class FGraph2DEdge;
	class FGraph3DEdge;
	class FDraftingComposite;
	enum class FModumateLayerType;
};

// Helper functions for creating / updating MOIs and their geometry.
// When multiple systems need to understand the relationship between control points/indices and MOI geometry,
// such as Tools, procedural meshes, icon generation, MOI interface implementations, etc., those functions can go here.
UCLASS(BlueprintType)
class MODUMATE_API UModumateObjectStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool GetPolygonProfile(const FBIMAssemblySpec *TrimAssembly, const FSimplePolygon*& OutProfile);

	static bool GetRelativeTransformOnPlanarObj(
		const AModumateObjectInstance *PlanarObj,
		const FVector &WorldPos, float DistanceFromBottom,
		bool bUseDistanceFromBottom, FVector2D &OutRelativePos,
		FQuat &OutRelativeRot);

	static bool GetWorldTransformOnPlanarObj(
		const AModumateObjectInstance *PlanarObj,
		const FVector2D &RelativePos, const FQuat &RelativeRot,
		FVector &OutWorldPos, FQuat &OutWorldRot);

	// Face-mounted objects
	static int32 GetParentFaceIndex(const AModumateObjectInstance *FaceMountedObj);
	static bool GetGeometryFromFaceIndex(const AModumateObjectInstance *Host, int32 FaceIndex,
		TArray<FVector>& OutFacePoints, FVector& OutNormal, FVector& OutFaceAxisX, FVector& OutFaceAxisY);
	static bool GetGeometryFromFaceIndex(const AModumateObjectInstance *Host, int32 FaceIndex,
		TArray<FVector>& OutFacePoints, FTransform& OutFaceOrigin);
	static bool GetGeometryFromSurfacePoly(const UModumateDocument* Doc, int32 SurfacePolyID, bool& bOutInterior, bool& bOutInnerBounds,
		FTransform& OutOrigin, TArray<FVector>& OutPerimeter, TArray<FPolyHole3D>& OutHoles);

	// Meta/Surface Objects
	static bool GetEdgeFaceConnections(const Modumate::FGraph3DEdge* GraphEdge, const UModumateDocument* Doc,
		bool& bOutConnectedToAnyFace, bool& bOutConnectedToValidFace);
	static bool GetEdgePolyConnections(const Modumate::FGraph2DEdge* SurfaceEdge, const UModumateDocument* Doc,
		bool& bOutConnectedToAnyPolygon, bool& bOutConnectedToValidPolygon);

	static bool GetNonPhysicalEnabledFlags(const AModumateObjectInstance* NonPhysicalMOI, bool& bOutVisible, bool& bOutCollisionEnabled);
	static bool GetMetaObjEnabledFlags(const AModumateObjectInstance* MetaMOI, bool& bOutVisible, bool& bOutCollisionEnabled);
	static bool GetSurfaceObjEnabledFlags(const AModumateObjectInstance* SurfaceMOI, bool& bOutVisible, bool& bOutCollisionEnabled);

	static void GetGraphIDsFromMOIs(const TSet<AModumateObjectInstance *> &MOIs, TSet<int32> &OutGraphObjIDs);

	// Given a plane hosted object, find some basic values:
	// the thickness of the assembly,
	// the distance along its normal vector from which the rest of its layers are positioned,
	// and the plane normal.
	static void GetPlaneHostedValues(const AModumateObjectInstance *PlaneHostedObj, float &OutThickness, float &OutStartOffset, FVector &OutNormal);

	// Given a plane hosted object, find the outermost delta vectors from the plane to its layer extents.
	// This does not include finishes or other hosted children (yet).
	static void GetExtrusionDeltas(const AModumateObjectInstance *PlaneHostedObj, FVector &OutStartDelta, FVector &OutEndDelta);

	// FF&E
	static bool GetMountedTransform(const FVector &MountOrigin, const FVector &MountNormal,
		const FVector &LocalDesiredNormal, const FVector &LocalDesiredTangent, FTransform &OutTransform);

	static bool GetFFEMountedTransform(const FBIMAssemblySpec *Assembly,
		const FSnappedCursor &SnappedCursor, FTransform &OutTransform);

	static bool GetFFEBoxSidePoints(const AActor *Actor, const FVector &AssemblyNormal, TArray<FVector> &OutPoints);

	// Extruded objects drafting.
	static bool GetExtrusionPerimeterPoints(const AModumateObjectInstance* MOI,
		const FVector& LineUp, const FVector& LineNormal, TArray<FVector>& outPerimeterPoints);
	static void GetExtrusionCutPlaneDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox, const TArray<FVector>& Perimeter,
		const FVector& StartPosition, const FVector& EndPosition, Modumate::FModumateLayerType LayerType, float Epsilon = 0.0f);
	static TArray<FEdge> GetExtrusionBeyondLinesFromMesh(const FPlane& Plane, const TArray<FVector>& Perimeter,
		const FVector& StartPosition, const FVector& EndPosition);
};
