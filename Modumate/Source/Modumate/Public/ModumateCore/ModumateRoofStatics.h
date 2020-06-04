// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Graph/ModumateGraph3DTypes.h"
#include "ModumateCore/ModumateTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Object.h"

#include "ModumateRoofStatics.generated.h"


namespace Modumate
{
	class FModumateDocument;
	class FModumateObjectInstance;

	namespace BIM
	{
		class FBIMPropertySheet;
	}
}

// Helper struct for roof tessellation
USTRUCT()
struct MODUMATE_API FTessellationPolygon
{
	GENERATED_USTRUCT_BODY();

	FTessellationPolygon() {};

	FTessellationPolygon(const FVector &InBaseUp, const FVector &InPolyNormal,
		const FVector &InStartPoint, const FVector &InStartDir,
		const FVector &InEndPoint, const FVector &InEndDir);

	FVector BaseUp, PolyNormal, StartPoint, StartDir, EndPoint, EndDir, CachedEdgeDir, CachedEdgeIntersection;

	FPlane CachedPlane;
	float CachedStartRayDist, CachedEndRayDist;
	bool bCachedEndsConverge;
	TArray<FVector> ClippingPoints, PolygonVerts;

	bool ClipWithPolygon(const FTessellationPolygon &ClippingPolygon);
	bool UpdatePolygonVerts();

	void DrawDebug(const FColor &Color, class UWorld* World = nullptr, const FTransform &Transform = FTransform::Identity);
};


// Helper struct for storing edge properties
// TODO: until more generic serialization works in BIM, this can only be used temporarily, rather than for serialized property storage
USTRUCT()
struct MODUMATE_API FRoofEdgeProperties
{
	GENERATED_USTRUCT_BODY()

	FRoofEdgeProperties()
		: bOverridden(false)
		, Slope(0.0f)
		, bHasFace(true)
		, Overhang(0.0f)
	{ }

	FRoofEdgeProperties(bool bInOverridden, float InSlope, bool bInHasFace, float InOverhang)
		: bOverridden(bInOverridden)
		, Slope(InSlope)
		, bHasFace(bInHasFace)
		, Overhang(InOverhang)
	{ }

	UPROPERTY()
	bool bOverridden;

	UPROPERTY()
	float Slope;

	UPROPERTY()
	bool bHasFace;

	UPROPERTY()
	float Overhang;
};


// Helper functions for accessing / editing roof data, shared between roof tools and objects
UCLASS(BlueprintType)
class MODUMATE_API UModumateRoofStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void InitializeProperties(Modumate::BIM::FBIMPropertySheet *RoofProperties, int32 NumEdges);

	static bool GetAllProperties(const Modumate::FModumateObjectInstance *RoofObject,
		TArray<FVector> &OutEdgePoints, TArray<int32> &OutEdgeIDs, FRoofEdgeProperties &OutDefaultProperties, TArray<FRoofEdgeProperties> &OutEdgeProperties);

	static bool SetAllProperties(Modumate::FModumateObjectInstance *RoofObject, const FRoofEdgeProperties &DefaultProperties, const TArray<FRoofEdgeProperties> &EdgeProperties);

	static bool GetEdgeProperties(const Modumate::FModumateObjectInstance *RoofObject, int32 EdgeID, FRoofEdgeProperties &OutProperties);

	static bool SetEdgeProperties(Modumate::FModumateObjectInstance *RoofObject, int32 EdgeID, const FRoofEdgeProperties &NewProperties);

	static bool UpdateRoofEdgeProperties(Modumate::FModumateObjectInstance *RoofObject, const TArray<int32> &NewEdgeIDs);

	static bool TessellateSlopedEdges(const TArray<FVector> &EdgePoints, const TArray<FRoofEdgeProperties> &EdgeProperties,
		TArray<FVector> &OutCombinedPolyVerts, TArray<int32> &OutPolyVertIndices, const FVector &NormalHint = FVector::UpVector);

	static FRoofEdgeProperties DefaultEdgeProperties;

private:
	static TArray<FVector> TempEdgePoints;
	static TArray<int32> TempEdgeIDs;

	static FRoofEdgeProperties TempDefaultEdgeProperties;
	static TArray<FRoofEdgeProperties> TempEdgeProperties;
	static TArray<FRoofEdgeProperties> TempCombinedEdgeProperties;
	static TArray<bool> TempEdgeOverrides;
	static TArray<float> TempEdgeSlopes;
	static TArray<bool> TempEdgesHaveFaces;
	static TArray<float> TempEdgeOverhangs;

	static TMap<int32, FRoofEdgeProperties> TempEdgePropertyMap;
};
