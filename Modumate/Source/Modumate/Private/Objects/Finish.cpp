// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Finish.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

FMOIFinishImpl::FMOIFinishImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
{ }

FMOIFinishImpl::~FMOIFinishImpl()
{
}

void FMOIFinishImpl::Destroy()
{
	MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags::Structure);
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

bool FMOIFinishImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
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

		MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags::Structure);
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
	int32 numPoints = CachedPerimeter.Num();
	FVector extrusionDelta = MOI->CalculateThickness() * GetNormal();

	for (int32 i = 0; i < numPoints; ++i)
	{
		int32 edgeIdxA = i;
		int32 edgeIdxB = (i + 1) % numPoints;

		FVector cornerMinA = CachedPerimeter[edgeIdxA];
		FVector cornerMinB = CachedPerimeter[edgeIdxB];
		FVector edgeDir = (cornerMinB - cornerMinA).GetSafeNormal();

		FVector cornerMaxA = cornerMinA + extrusionDelta;
		FVector cornerMaxB = cornerMinB + extrusionDelta;

		outPoints.Add(FStructurePoint(cornerMinA, edgeDir, edgeIdxA));

		outLines.Add(FStructureLine(cornerMinA, cornerMinB, edgeIdxA, edgeIdxB));
		outLines.Add(FStructureLine(cornerMaxA, cornerMaxB, edgeIdxA + numPoints, edgeIdxB + numPoints));
		outLines.Add(FStructureLine(cornerMinA, cornerMaxA, edgeIdxA, edgeIdxA + numPoints));
	}
}

void FMOIFinishImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller)
{
	// parent handles
	FModumateObjectInstance* parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTSurfacePolygon)))
	{
		return;
	}

	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto cornerHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		cornerHandle->SetTargetIndex(i);
		cornerHandle->SetTargetMOI(parent);

		auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetAdjustPolyEdge(true);
		edgeHandle->SetTargetMOI(parent);
	}
}

void FMOIFinishImpl::UpdateConnectedEdges()
{
	CachedParentConnectedMOIs.Reset();

	const FModumateObjectInstance* polyParent = MOI ? MOI->GetParentObject() : nullptr;
	if (polyParent)
	{
		polyParent->GetConnectedMOIs(CachedParentConnectedMOIs);
	}

	CachedConnectedEdgeChildren.Reset();
	for (FModumateObjectInstance* polyConnectedMOI : CachedParentConnectedMOIs)
	{
		if (polyConnectedMOI && (polyConnectedMOI->GetObjectType() == EObjectType::OTSurfaceEdge))
		{
			CachedConnectedEdgeChildren.Append(polyConnectedMOI->GetChildObjects());
		}
	}
}

void FMOIFinishImpl::MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags DirtyFlags)
{
	UpdateConnectedEdges();

	for (FModumateObjectInstance* connectedEdgeChild : CachedConnectedEdgeChildren)
	{
		connectedEdgeChild->MarkDirty(DirtyFlags);
	}
}
