// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceFinish.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateObjectInstance.h"


namespace Modumate
{
	FMOIFinishImpl::FMOIFinishImpl(FModumateObjectInstance *moi)
		: FDynamicModumateObjectInstanceImpl(moi)
		, CachedNormal(ForceInitToZero)
	{ }

	FMOIFinishImpl::~FMOIFinishImpl()
	{
	}

	FVector FMOIFinishImpl::GetCorner(int32 index) const
	{
		const FModumateObjectInstance *parentObj = MOI->GetParentObject();
		if (!ensureMsgf(parentObj, TEXT("Finish ID %d does not have parent object!"), MOI->ID))
		{
			return GetLocation();
		}

		float thickness = MOI->CalculateThickness();
		int32 numCP = MOI->GetControlPoints().Num();
		FVector cornerOffset = (index < numCP) ? FVector::ZeroVector : (CachedNormal * thickness);

		return MOI->GetControlPoint(index % numCP) + cornerOffset;
	}

	FVector FMOIFinishImpl::GetNormal() const
	{
		return CachedNormal;
	}

	bool FMOIFinishImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
	{
		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
			// For now, finishes are entirely defined by their mount to their parent wall,
			// and they inherit any bored holes, so we can just leave their setup to Visual cleaning and dirty it here.
			MOI->MarkDirty(EObjectDirtyFlags::Visuals);
			break;
		case EObjectDirtyFlags::Visuals:
			SetupDynamicGeometry();
			MOI->UpdateVisibilityAndCollision();
			break;
		default:
			break;
		}

		return true;
	}

	void FMOIFinishImpl::SetupDynamicGeometry()
	{
		// Clear adjustment handle without clearing the object tag
		// TODO: why is this necessary?
		MOI->ClearAdjustmentHandles();

		FModumateObjectInstance *surfacePolyParent = MOI->GetParentObject();
		if (!ensure(surfacePolyParent && (surfacePolyParent->GetObjectType() == EObjectType::OTSurfacePolygon)))
		{
			return;
		}

		MOI->SetControlPoints(surfacePolyParent->GetControlPoints());
		CachedNormal = surfacePolyParent->GetNormal();

		if ((MOI->GetControlPoints().Num() < 3) || !CachedNormal.IsNormalized())
		{
			return;
		}

		bool bToleratePlanarErrors = true;
		bool bLayerSetupSuccess = DynamicMeshActor->CreateBasicLayerDefs(MOI->GetControlPoints(), CachedNormal, MOI->GetAssembly(),
			0.0f, FVector::ZeroVector, 0.0f, bToleratePlanarErrors);

		if (bLayerSetupSuccess)
		{
			DynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);
		}
	}

	void FMOIFinishImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
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
}
