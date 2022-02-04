// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Graph/Graph3DTypes.h"
#include "Graph/Graph2DTypes.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/ModumateObjectEnums.h"
#include "Objects/DimensionOffset.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UObject/Object.h"

#include "ModumateObjectStatics.generated.h"


struct FBIMAssemblySpec;
class UModumateDocument;
class AModumateObjectInstance;
struct FSimplePolygon;

class FGraph2DEdge;
class FGraph3DEdge;
class FDraftingComposite;
enum class FModumateLayerType;

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
	static bool GetEdgeFaceConnections(const FGraph3DEdge* GraphEdge, const UModumateDocument* Doc,
		bool& bOutConnectedToAnyFace, bool& bOutConnectedToVisibleFace, bool& bOutConnectedToVisibleChild);
	static bool GetEdgePolyConnections(const FGraph2DEdge* SurfaceEdge, const UModumateDocument* Doc,
		bool& bOutConnectedToAnyPolygon, bool& bOutConnectedToVisiblePolygon, bool& bOutConnectedToVisibleChild);

	static bool GetNonPhysicalEnabledFlags(const AModumateObjectInstance* NonPhysicalMOI, bool& bOutVisible, bool& bOutCollisionEnabled);
	static bool GetMetaObjEnabledFlags(const AModumateObjectInstance* MetaMOI, bool& bOutVisible, bool& bOutCollisionEnabled);
	static bool GetSurfaceObjEnabledFlags(const AModumateObjectInstance* SurfaceMOI, bool& bOutVisible, bool& bOutCollisionEnabled);

	static void GetGraphIDsFromMOIs(const TSet<AModumateObjectInstance *> &MOIs, TSet<int32> &OutGraphObjIDs);

	// Given a plane hosted object, find some basic values:
	// the thickness of the assembly,
	// the distance along its normal vector from which the rest of its layers are positioned,
	// and the plane normal.
	static bool GetPlaneHostedValues(const AModumateObjectInstance *PlaneHostedObj, float &OutThickness, float &OutStartOffset, FVector &OutNormal);

	// Given a plane hosted object, find the outermost delta vectors from the plane to its layer extents.
	// This does not include finishes or other hosted children (yet).
	static bool GetExtrusionDeltas(const AModumateObjectInstance *PlaneHostedObj, FVector &OutStartDelta, FVector &OutEndDelta);

	// FF&E
	static bool GetMountedTransform(const FVector &MountOrigin, const FVector &MountNormal,
		const FVector &LocalDesiredNormal, const FVector &LocalDesiredTangent, FTransform &OutTransform);

	static bool GetFFEMountedTransform(const FBIMAssemblySpec *Assembly,
		const FSnappedCursor &SnappedCursor, FTransform &OutTransform);

	static bool GetFFEBoxSidePoints(const AActor *Actor, const FVector &AssemblyNormal, TArray<FVector> &OutPoints);

	// Extruded objects polygon utilities

	// OffsetX/OffsetY and FlipSigns have their X and Y components set to correspond to the X and Y components of the profile polygon.
	//     The case of a Trim extruded along the bottom of a vertical wall would then have the names:
	//     - "Normal" axis pointing in world XY and corresponding to the polygon's X axis
	//     - "Up" axis pointing in world +Z and corresponding to the polygon's Y axis
	// GetExtrusionProfilePoints centrally interprets a polygon's shape and the assembly's specified scale factor, and outputs
	// OutProfilePoints in the polygon's coordinate axes, where the origin represents the hosting line.
	static bool GetExtrusionProfilePoints(const FBIMAssemblySpec& Assembly, const FDimensionOffset& OffsetX, const FDimensionOffset& OffsetY,
		const FVector2D& FlipSigns, TArray<FVector2D>& OutProfilePoints, FBox2D& OutProfileExtents);

	// GetExtrusionObjectPoints first interprets an extrusion in polygon space, and then outputs
	// OutObjectPoints in the object's specified coordinate axes (which may be rotated by the object beforehand).
	// OutObjectPoints are expected only be offset from world space by a position along the hosting line.
	static bool GetExtrusionObjectPoints(const FBIMAssemblySpec& Assembly, const FVector& LineNormal, const FVector& LineUp,
		const FDimensionOffset& OffsetX, const FDimensionOffset& OffsetY, const FVector2D& FlipSigns, TArray<FVector>& OutObjectPoints);

	// Extruded objects drafting
	static void GetExtrusionCutPlaneDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox, const TArray<FVector>& Perimeter,
		const FVector& StartPosition, const FVector& EndPosition, FModumateLayerType LayerType, float Epsilon = 0.0f);
	static TArray<FEdge> GetExtrusionBeyondLinesFromMesh(const FPlane& Plane, const TArray<FVector>& Perimeter,
		const FVector& StartPosition, const FVector& EndPosition);

	static void GetTerrainSurfaceObjectEnabledFlags(const AModumateObjectInstance* TerrainSurfaceObj, bool& bOutVisible, bool& bOutCollisionEnabled);

	static bool IsObjectInGroup(const UModumateDocument* Doc, const AModumateObjectInstance* Object, int32 GroupID = MOD_ID_NONE);
	static bool IsObjectInSubgroup(const UModumateDocument* Doc, const AModumateObjectInstance* Object, int32 ActiveGroup,
		int32& OutSubgroup, bool& bOutIsInGroup);
	static bool GetGroupIdsForGroupChange(const UModumateDocument* Doc, int32 NewgroupID, TArray<int32>& OutAffectedGroups);
	static void GetObjectsInGroups(UModumateDocument* Doc, const TArray<int32>& GroupIDs, TSet<AModumateObjectInstance*>& OutObjects);
	static void HideObjectsInGroups(UModumateDocument* Doc, const TArray<int32>& GroupIDs, bool bHide=true);

	static void GetDesignOptionsForGroup(UModumateDocument* Doc, int32 GroupID, TArray<int32>& OutDesignOptionIDs);
	static void UpdateDesignOptionVisibility(UModumateDocument* Doc);

	static void GetWebMOIArrayForObjects(const TArray<const AModumateObjectInstance*>& Objects, FString& OutJson);

private:
	static bool GetGroupIdsForGroupChangeHelper(const UModumateDocument* Doc, int32 NewGroupID, int32 OldGroupID, TArray<int32>& OutAffectedGroups, bool& bOutFoundOldGroup);
};

UCLASS()
class MODUMATE_API UModumateTypeStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EObjectType ObjectTypeFromToolMode(EToolMode tm);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EToolMode ToolModeFromObjectType(EObjectType ot);

	static EGraph3DObjectType Graph3DObjectTypeFromObjectType(EObjectType ot);
	static EObjectType ObjectTypeFromGraph3DType(EGraph3DObjectType GraphType);

	static EGraphObjectType Graph2DObjectTypeFromObjectType(EObjectType ObjectType);
	static EObjectType ObjectTypeFromGraph2DType(EGraphObjectType GraphType, EToolCategories GraphCategory);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetTextForObjectType(EObjectType ObjectType, bool bPlural = false);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static ECollisionChannel CollisionTypeFromObjectType(EObjectType ot);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetAreaTypeText(EAreaType AreaType);

	static const EObjectDirtyFlags OrderedDirtyFlags[3];

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EToolCategories GetToolCategory(EToolMode ToolMode);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EToolCategories GetObjectCategory(EObjectType ObjectType);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetToolCategoryText(EToolCategories ToolCategory);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static TArray<EObjectType> GetObjectTypeWithDirectionIndicator();
};