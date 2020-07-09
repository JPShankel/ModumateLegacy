// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/PlaneBase.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Common/EditModelPolyAdjustmentHandles.h"

namespace Modumate
{
	FMOIPlaneImplBase::FMOIPlaneImplBase(FModumateObjectInstance *moi)
		: FDynamicModumateObjectInstanceImpl(moi)
		, CachedPlane(ForceInitToZero)
		, CachedAxisX(ForceInitToZero)
		, CachedAxisY(ForceInitToZero)
		, CachedOrigin(ForceInitToZero)
		, CachedCenter(ForceInitToZero)
	{
		bDrawHUDTags = false;
	}

	FVector FMOIPlaneImplBase::GetCorner(int32 index) const
	{
		int32 numCP = MOI->GetControlPoints().Num();
		if (ensure(index < numCP))
		{
			return MOI->GetControlPoint(index);
		}

		return GetLocation();
	}

	FQuat FMOIPlaneImplBase::GetRotation() const
	{
		return FRotationMatrix::MakeFromXY(CachedAxisX, CachedAxisY).ToQuat();
	}

	void FMOIPlaneImplBase::SetRotation(const FQuat& r)
	{
		if (DynamicMeshActor.IsValid())
		{
			DynamicMeshActor->SetActorRotation(FQuat::Identity);
		}
	}

	FVector FMOIPlaneImplBase::GetLocation() const
	{
		return CachedCenter;
	}

	FVector FMOIPlaneImplBase::GetNormal() const
	{
		return CachedPlane;
	}

	TArray<FModelDimensionString> FMOIPlaneImplBase::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;
		for (int32 i = 0, numCP = MOI->GetControlPoints().Num(); i < numCP; ++i)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->GetControlPoint(i);
			ds.Point2 = MOI->GetControlPoint((i + 1) % numCP);
			ds.Functionality = EEnterableField::None;
			ds.Offset = 50;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ds.Owner = MOI->GetActor();
			ret.Add(ds);
		}
		return ret;
	}

	void FMOIPlaneImplBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		// Don't return points or lines if we're snapping,
		// since otherwise the plane will interfere with edges and vertices.
		if (bForSnapping)
		{
			return;
		}

		int32 numPolyPoints = MOI->GetControlPoints().Num();

		for (int32 i = 0; i < numPolyPoints; ++i)
		{
			int32 nextI = (i + 1) % numPolyPoints;
			const FVector &cp1 = MOI->GetControlPoint(i);
			const FVector &cp2 = MOI->GetControlPoint(nextI);
			FVector dir = (cp2 - cp1).GetSafeNormal();

			outPoints.Add(FStructurePoint(cp1, dir, i));
			outLines.Add(FStructureLine(cp1, cp2, i, nextI));
		}
	}

	// Adjustment Handles
	void FMOIPlaneImplBase::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (MOI->HasAdjustmentHandles())
		{
			return;
		}

		int32 numCP = MOI->GetControlPoints().Num();

		for (int32 i = 0; i < numCP; ++i)
		{
			auto vertexHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
			vertexHandle->SetTargetIndex(i);

			auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
			edgeHandle->SetTargetIndex(i);
			edgeHandle->SetAdjustPolyEdge(true);
		}
	}

	bool FMOIPlaneImplBase::ShowStructureOnSelection() const
	{
		return false;
	}

	float FMOIPlaneImplBase::GetAlpha() const
	{
		return MOI->IsHovered() ? 1.5f : 1.0f;
	}
}
