// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstancePlaneBase.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPolyAdjustmentHandles.h"

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
		int32 numCP = MOI->ControlPoints.Num();
		if (ensure(index < numCP))
		{
			return MOI->ControlPoints[index];
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
		for (int32 i = 0, numCP = MOI->ControlPoints.Num(); i < numCP; ++i)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->ControlPoints[i];
			ds.Point2 = MOI->ControlPoints[(i + 1) % numCP];
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

		int32 numPolyPoints = MOI->ControlPoints.Num();

		for (int32 i = 0; i < numPolyPoints; ++i)
		{
			int32 nextI = (i + 1) % numPolyPoints;
			const FVector &cp1 = MOI->ControlPoints[i];
			const FVector &cp2 = MOI->ControlPoints[nextI];
			FVector dir = (cp2 - cp1).GetSafeNormal();

			outPoints.Add(FStructurePoint(cp1, dir, i));
			outLines.Add(FStructureLine(cp1, cp2, i, nextI));
		}
	}

	// Adjustment Handles
	void FMOIPlaneImplBase::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		if (AdjustmentHandles.Num() > 0)
		{
			return;
		}
		auto makeActor = [this, controller](IAdjustmentHandleImpl *impl, UStaticMesh *mesh, const FVector &s, const TArray<int32>& CP, float offsetDist, const int32& side = -1)
		{
			AAdjustmentHandleActor_CPP *actor = DynamicMeshActor->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			actor->SetActorMesh(mesh);
			actor->SetHandleScale(s);
			actor->SetHandleScaleScreenSize(s);
			actor->Side = side;
			if (CP.Num() > 0)
			{
				actor->SetPolyHandleSide(CP, MOI, offsetDist);
			}

			impl->Handle = actor;
			actor->Implementation = impl;
			actor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
			AdjustmentHandles.Add(actor);
		};

		for (size_t i = 0; i < MOI->ControlPoints.Num(); ++i)
		{
			makeActor(new FAdjustPolyPointHandle(MOI, i, (i + 1) % MOI->ControlPoints.Num()), AEditModelGameMode_CPP::FaceAdjusterMesh, FVector(0.0015f, 0.0015f, 0.0015f), TArray<int32>{int32(i), int32(i + 1) % MOI->ControlPoints.Num()}, 16.0f);
		}
	};

	bool FMOIPlaneImplBase::ShowStructureOnSelection() const
	{
		return false;
	}

	float FMOIPlaneImplBase::GetAlpha() const
	{
		return MOI->IsHovered() ? 1.5f : 1.0f;
	}
}
