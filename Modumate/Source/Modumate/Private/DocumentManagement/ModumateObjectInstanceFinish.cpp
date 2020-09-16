// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceFinish.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateObjectInstance.h"

FMOIFinishImpl::FMOIFinishImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
{ }

FMOIFinishImpl::~FMOIFinishImpl()
{
}

FVector FMOIFinishImpl::GetCorner(int32 index) const
{
	const FModumateObjectInstance *parentObj = MOI->GetParentObject();
	if (!ensure(parentObj) || (CachedPerimeter.Num() == 0))
	{
		return FVector::ZeroVector;
	}

	float thickness = MOI->CalculateThickness();
	int32 numPoints = CachedPerimeter.Num();
	FVector cornerOffset = (index < numPoints) ? FVector::ZeroVector : (GetNormal() * thickness);

	return CachedPerimeter[index % numPoints] + cornerOffset;
}

FVector FMOIFinishImpl::GetNormal() const
{
	return CachedGraphOrigin.GetRotation().GetAxisZ();
}

bool FMOIFinishImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		// The finish requires a surface polygon parent from which to extrude its layered assembly
		FModumateObjectInstance *surfacePolyParent = MOI->GetParentObject();
		if ((surfacePolyParent == nullptr) || !ensure(surfacePolyParent->GetObjectType() == EObjectType::OTSurfacePolygon))
		{
			return false;
		}

		bool bInteriorPolygon, bInnerBoundsPolygon;
		if (!UModumateObjectStatics::GetGeometryFromSurfacePoly(MOI->GetDocument(), surfacePolyParent->ID,
			bInteriorPolygon, bInnerBoundsPolygon, CachedGraphOrigin, CachedPerimeter, CachedHoles))
		{
			return false;
		}

		// We shouldn't have been able to parent a finish to an invalid surface polygon, so don't bother trying to get geometry from it
		if (!ensure(bInteriorPolygon && !bInnerBoundsPolygon) || (CachedPerimeter.Num() < 3))
		{
			return false;
		}

		bool bToleratePlanarErrors = true;
		bool bLayerSetupSuccess = DynamicMeshActor->CreateBasicLayerDefs(CachedPerimeter, CachedGraphOrigin.GetRotation().GetAxisZ(), CachedHoles,
			MOI->GetAssembly(), 0.0f, FVector::ZeroVector, 0.0f, bToleratePlanarErrors);

		if (bLayerSetupSuccess)
		{
			DynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);
		}
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

void FMOIFinishImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	int32 numCP = MOI->GetControlPoints().Num();

	for (int32 i = 0; i < numCP; ++i)
	{
		int32 edgeIdxA = i;
		int32 edgeIdxB = (i + 1) % numCP;

		FVector cornerMinA = GetCorner(edgeIdxA);
		FVector cornerMinB = GetCorner(edgeIdxB);
		FVector edgeDir = (cornerMinB - cornerMinA).GetSafeNormal();

		FVector cornerMaxA = GetCorner(edgeIdxA + numCP);
		FVector cornerMaxB = GetCorner(edgeIdxB + numCP);

		outPoints.Add(FStructurePoint(cornerMinA, edgeDir, edgeIdxA));

		outLines.Add(FStructureLine(cornerMinA, cornerMinB, edgeIdxA, edgeIdxB));
		outLines.Add(FStructureLine(cornerMaxA, cornerMaxB, edgeIdxA + numCP, edgeIdxB + numCP));
		outLines.Add(FStructureLine(cornerMinA, cornerMaxA, edgeIdxA, edgeIdxA + numCP));
	}
}

