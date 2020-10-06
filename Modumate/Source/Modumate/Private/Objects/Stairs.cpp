// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Stairs.h"

#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "ModumateCore/ModumateUnits.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"

class AEditModelPlayerController_CPP;


FMOIStaircaseImpl::FMOIStaircaseImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, bCachedStartRiser(true)
	, bCachedEndRiser(true)
{
	const auto& assemblySpec = MOI->GetAssembly();
	CachedTreadDims.UpdateLayersFromAssembly(assemblySpec.TreadLayers);
	CachedRiserDims.UpdateLayersFromAssembly(assemblySpec.RiserLayers);
	bCachedUseRisers = assemblySpec.RiserLayers.Num() != 0;
	TreadRun = assemblySpec.TreadDepth.AsWorldCentimeters();
}

FMOIStaircaseImpl::~FMOIStaircaseImpl()
{}

bool FMOIStaircaseImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		SetupDynamicGeometry();
		break;

	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		break;

	default:
		break;
	}

	return true;
}

void FMOIStaircaseImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	// TODO: use the tread/riser polygons as structural points and lines

	FModumateObjectInstance *planeParent = MOI ? MOI->GetParentObject() : nullptr;
	if (planeParent)
	{
		planeParent->GetStructuralPointsAndLines(outPoints, outLines, true);
	}
}

void FMOIStaircaseImpl::SetupDynamicGeometry()
{
	FModumateObjectInstance* planeParent = MOI ? MOI->GetParentObject() : nullptr;
	if (!(planeParent && (planeParent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	TArray<FVector> runPlanePoints;
	for (int32 pointIdx = 0; pointIdx < planeParent->GetNumCorners(); ++pointIdx)
	{
		runPlanePoints.Add(planeParent->GetCorner(pointIdx));
	}

	// Tread 'depth' is horizontal run from nose to nose.
	float goalTreadDepth = TreadRun;

	// Calculate the polygons that make up the outer surfaces of each tread and riser of the linear stair run
	float stepRun, stepRise;
	FVector runDir(ForceInitToZero), stairOrigin(ForceInitToZero);
	if (!Modumate::FStairStatics::CalculateLinearRunPolysFromPlane(
		runPlanePoints, goalTreadDepth, bCachedUseRisers, bCachedStartRiser, bCachedEndRiser,
		stepRun, stepRise, runDir, stairOrigin,
		CachedTreadPolys, CachedRiserPolys))
	{
		return;
	}

	TreadLayers.Empty();
	RiserLayers.Empty();
	const float totalTreadThickness = CachedTreadDims.TotalUnfinishedWidth;
	const float totalRiserThickness = bCachedUseRisers ? CachedRiserDims.TotalUnfinishedWidth :
		2.0f * Modumate::InchesToCentimeters;  // Empirically derived overlap. 
	const FBIMAssemblySpec& assembly = MOI->GetAssembly();
	int32 numTreadLayers = assembly.TreadLayers.Num();
	int32 numRiserLayers = assembly.RiserLayers.Num();
	float currentTreadThickness = 0.0f;
	float currentRiserThickness = 0.0f;

	TArray<FVector> pointsA(&FVector::ZeroVector, 4);
	TArray<FVector> pointsB(&FVector::ZeroVector, 4);
	for (int32 layer = 0; layer < numTreadLayers; ++layer)
	{
		float treadThickness = assembly.TreadLayers[layer].Thickness.AsWorldCentimeters();
		pointsA[0] = CachedTreadPolys[0][0] - currentTreadThickness * FVector::UpVector;
		pointsA[1] = CachedTreadPolys[0][1] - currentTreadThickness * FVector::UpVector;
		pointsA[2] = CachedTreadPolys[0][2] - currentTreadThickness * FVector::UpVector + totalRiserThickness * runDir;
		pointsA[3] = CachedTreadPolys[0][3] - currentTreadThickness * FVector::UpVector + totalRiserThickness * runDir;
		currentTreadThickness += treadThickness;

		for (int32 p = 0; p < 4; ++p)
		{
			pointsB[p] = pointsA[p] - treadThickness * FVector::UpVector;
		}
		FLayerGeomDef& treadLayer = TreadLayers.Emplace_GetRef(pointsA, pointsB, -FVector::UpVector, -FVector::ForwardVector);
	}

	for (int32 layer = 0; layer < numRiserLayers; ++layer)
	{
		float riserThickness = assembly.RiserLayers[layer].Thickness.AsWorldCentimeters();
		pointsA[0] = CachedRiserPolys[0][0] + currentRiserThickness * runDir;
		pointsA[1] = CachedRiserPolys[0][1] + currentRiserThickness * runDir;
		pointsA[2] = CachedRiserPolys[0][2] + currentRiserThickness * runDir - totalTreadThickness * FVector::UpVector;
		pointsA[3] = CachedRiserPolys[0][3] + currentRiserThickness * runDir - totalTreadThickness * FVector::UpVector;
		currentRiserThickness += riserThickness;

		for (int32 p = 0; p < 4; ++p)
		{
			pointsB[p] = pointsA[p] + riserThickness * runDir;
		}
		RiserLayers.Emplace(pointsA, pointsB, runDir, (CachedRiserPolys[0][1] - CachedRiserPolys[0][0]).GetSafeNormal());
	}


	// For linear stairs, each riser has the same normal, so populate them here
	int32 numRisers = CachedRiserPolys.Num();
	CachedRiserNormals.SetNum(numRisers);
	for (int32 i = 0; i < numRisers; ++i)
	{
		CachedRiserNormals[i] = runDir;
	}

	// Set up the triangulated staircase mesh by extruding each tread and riser polygon
	DynamicMeshActor->SetupStairPolys(stairOrigin, CachedTreadPolys, CachedRiserPolys, CachedRiserNormals, TreadLayers, RiserLayers,
		MOI->GetAssembly());
}

void FMOIStaircaseImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	FModumateObjectInstance *parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	// Make the polygon adjustment handles, for modifying the parent plane's polygonal shape
	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetAdjustPolyEdge(true);
		edgeHandle->SetTargetMOI(parent);
	}
}
