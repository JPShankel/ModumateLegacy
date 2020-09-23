// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Stairs.h"

#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "ModumateCore/ModumateUnits.h"

class AEditModelPlayerController_CPP;


FMOIStaircaseImpl::FMOIStaircaseImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, bCachedUseRisers(true)
	, bCachedStartRiser(true)
	, bCachedEndRiser(true)
{
	// TODO: get these riser configuration values from an assembly, and/or allow overriding them as instance parameters via handles/tools/menus
}

FMOIStaircaseImpl::~FMOIStaircaseImpl()
{}

bool FMOIStaircaseImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		FModumateObjectInstance* planeParent = MOI ? MOI->GetParentObject() : nullptr;
		if (!(planeParent && (planeParent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			return false;
		}

		TArray<FVector> runPlanePoints;
		for (int32 pointIdx = 0; pointIdx < planeParent->GetNumCorners(); ++pointIdx)
		{
			runPlanePoints.Add(planeParent->GetCorner(pointIdx));
		}

		// TODO: get this material from a real assembly
		FArchitecturalMaterial material;
		material.EngineMaterial = DynamicMeshActor->StaircaseMaterial;

		// TODO: get these values from a real assembly
		float goalRunDepth = Modumate::Units::FUnitValue(12.0f, Modumate::Units::EUnitType::WorldInches).AsWorldCentimeters();
		float treadThickness = Modumate::Units::FUnitValue(1.0f, Modumate::Units::EUnitType::WorldInches).AsWorldCentimeters();

		// Calculate the polygons that make up the outer surfaces of each tread and riser of the linear stair run
		float stepRun, stepRise;
		FVector runDir(ForceInitToZero), stairOrigin(ForceInitToZero);
		if (!Modumate::FStairStatics::CalculateLinearRunPolysFromPlane(
			runPlanePoints, goalRunDepth, bCachedUseRisers, bCachedStartRiser, bCachedEndRiser,
			stepRun, stepRise, runDir, stairOrigin,
			CachedTreadPolys, CachedRiserPolys))
		{
			return false;
		}

		// For linear stairs, each riser has the same normal, so populate them here
		int32 numRisers = CachedRiserPolys.Num();
		CachedRiserNormals.SetNum(numRisers);
		for (int32 i = 0; i < numRisers; ++i)
		{
			CachedRiserNormals[i] = runDir;
		}

		// Set up the triangulated staircase mesh by extruding each tread and riser polygon
		return DynamicMeshActor->SetupStairPolys(stairOrigin, CachedTreadPolys, CachedRiserPolys, CachedRiserNormals, treadThickness, material);
	}
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
