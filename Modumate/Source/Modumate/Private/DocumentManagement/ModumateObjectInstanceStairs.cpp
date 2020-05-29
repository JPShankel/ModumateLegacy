// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceStairs.h"

#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "ModumateCore/ModumateUnits.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{

	FMOIStaircaseImpl::FMOIStaircaseImpl(FModumateObjectInstance *moi)
		: FDynamicModumateObjectInstanceImpl(moi)
		, bCachedUseRisers(true)
		, bCachedStartRiser(true)
		, bCachedEndRiser(true)
	{
		// TODO: get these riser configuration values from an assembly, and/or allow overriding them as instance parameters via handles/tools/menus
	}

	FMOIStaircaseImpl::~FMOIStaircaseImpl()
	{}

	void FMOIStaircaseImpl::SetupDynamicGeometry()
	{
		FModumateObjectInstance *planeParent = MOI ? MOI->GetParentObject() : nullptr;
		if (planeParent && (planeParent->GetObjectType() == EObjectType::OTMetaPlane))
		{
			// TODO: get this material from a real assembly
			FArchitecturalMaterial material;
			material.EngineMaterial = DynamicMeshActor->StaircaseMaterial;

			// TODO: get these values from a real assembly
			float goalRunDepth = Units::FUnitValue(12.0f, Units::EUnitType::WorldInches).AsWorldCentimeters();
			float treadThickness = Units::FUnitValue(1.0f, Units::EUnitType::WorldInches).AsWorldCentimeters();

			// Calculate the polygons that make up the outer surfaces of each tread and riser of the linear stair run
			float stepRun, stepRise;
			FVector runDir(ForceInitToZero), stairOrigin(ForceInitToZero);
			bool bStairSuccess = FStairStatics::CalculateLinearRunPolysFromPlane(
				planeParent->GetControlPoints(), goalRunDepth, bCachedUseRisers, bCachedStartRiser, bCachedEndRiser,
				stepRun, stepRise, runDir, stairOrigin,
				CachedTreadPolys, CachedRiserPolys);

			// For linear stairs, each riser has the same normal, so populate them here
			int32 numRisers = CachedRiserPolys.Num();
			if (bStairSuccess)
			{
				CachedRiserNormals.SetNum(numRisers);
				for (int32 i = 0; i < numRisers; ++i)
				{
					CachedRiserNormals[i] = runDir;
				}
			}

			// Set up the triangulated staircase mesh by extruding each tread and riser polygon
			bStairSuccess = bStairSuccess && DynamicMeshActor->SetupStairPolys(
				stairOrigin, CachedTreadPolys, CachedRiserPolys, CachedRiserNormals, treadThickness, material);

			if (bStairSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("Stairs: num treads: %d, num risers: %d, step run: %.2fcm, step rise: %.2fcm"),
					CachedTreadPolys.Num(), numRisers, stepRun, stepRise);
			}
		}
	}

	void FMOIStaircaseImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
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

	TArray<FModelDimensionString> FMOIStaircaseImpl::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;

		return ret;
	}
}
