// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Graph/Graph3DTypes.h"
#include "ModumateCore/ModumateTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Object.h"

#include "ModumateRoofStatics.generated.h"

class UModumateDocument;
class AModumateObjectInstance;

struct FBIMPropertySheet;

// Helper struct for roof tessellation
USTRUCT()
struct MODUMATE_API FTessellationPolygon
{
	GENERATED_USTRUCT_BODY();

	FTessellationPolygon();

	FTessellationPolygon(const FVector &InBaseUp, const FVector &InPolyNormal,
		const FVector &InStartPoint, const FVector &InStartDir,
		const FVector &InEndPoint, const FVector &InEndDir,
		float InStartExtensionDist, float InEndExtensionDist);

	FVector BaseUp, PolyNormal, StartPoint, StartDir, EndPoint, EndDir, CachedEdgeDir, CachedEdgeIntersection;

	FPlane CachedPlane;
	float CachedStartRayDist, CachedEndRayDist;
	bool bCachedEndsConverge;
	TArray<FVector> ClippingPoints, PolygonVerts;
	float StartExtensionDist, EndExtensionDist;

	bool ClipWithPolygon(const FTessellationPolygon &ClippingPolygon);
	bool UpdatePolygonVerts();
	bool IsValid() const;

	void DrawDebug(const FColor &Color, class UWorld* World = nullptr, const FTransform &Transform = FTransform::Identity);
};


// Helper struct for storing roof perimeter edge properties
USTRUCT()
struct MODUMATE_API FRoofEdgeProperties
{
	GENERATED_USTRUCT_BODY()

	FRoofEdgeProperties();
	FRoofEdgeProperties(bool bInOverridden, float InSlope, bool bInHasFace, float InOverhang);

	UPROPERTY()
	bool bOverridden;

	UPROPERTY()
	float Slope;

	UPROPERTY()
	bool bHasFace;

	UPROPERTY()
	float Overhang;
};

USTRUCT()
struct MODUMATE_API FMOIRoofPerimeterData
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<int32, FRoofEdgeProperties> EdgeProperties;
};


// Helper functions for accessing / editing roof data, shared between roof tools and objects
UCLASS(BlueprintType)
class MODUMATE_API UModumateRoofStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void InitializeProperties(FBIMPropertySheet *RoofProperties, int32 NumEdges);

	static bool GetAllProperties(const AModumateObjectInstance *RoofObject,
		TArray<FVector> &OutEdgePoints, TArray<FGraphSignedID> &OutEdgeIDs, FRoofEdgeProperties &OutDefaultProperties, TArray<FRoofEdgeProperties> &OutEdgeProperties);

	static bool TessellateSlopedEdges(const TArray<FVector> &EdgePoints, const TArray<FRoofEdgeProperties> &EdgeProperties,
		TArray<FVector> &OutCombinedPolyVerts, TArray<int32> &OutPolyVertIndices, const FVector &NormalHint = FVector::UpVector, UWorld *DebugDrawWorld = nullptr);

	static FRoofEdgeProperties DefaultEdgeProperties;
};
