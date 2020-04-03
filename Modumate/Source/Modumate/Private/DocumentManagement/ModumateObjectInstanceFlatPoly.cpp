// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceFlatPoly.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelAdjustmentHandleBase.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPolyAdjustmentHandles.h"
#include "HUDDrawWidget_CPP.h"
#include "LineActor3D_CPP.h"
#include "ModumateGeometryStatics.h"

class AEditModelPlayerController_CPP;


namespace Modumate
{
	FMOIFlatPolyImpl::FMOIFlatPolyImpl(FModumateObjectInstance *moi, bool wantsInvertHandle) : FDynamicModumateObjectInstanceImpl(moi), WantsInvertHandle(wantsInvertHandle) {}

	FMOIFlatPolyImpl::~FMOIFlatPolyImpl()
	{
	}

	FVector FMOIFlatPolyImpl::GetCorner(int32 index) const
	{
		float thickness = MOI->CalculateThickness();
		int32 numCP = MOI->ControlPoints.Num();
		FVector extrusionDir = FVector::UpVector;

		FPlane plane;
		UModumateGeometryStatics::GetPlaneFromPoints(MOI->ControlPoints, plane);

		// For now, make sure that the plane of the flat poly MOI is horizontal
		if (ensureAlways((numCP >= 3) && FVector::Parallel(extrusionDir, plane)))
		{
			FVector corner = MOI->ControlPoints[index % numCP];

			if (index >= numCP)
			{
				corner += thickness * extrusionDir;
			}

			return corner;
		}
		else
		{
			return GetLocation();
		}
	}

	void FMOIFlatPolyImpl::InvertObject()
	{
		if (MOI->ObjectInverted)
		{
			for (auto &cp : MOI->ControlPoints)
			{
				//cp.Z -= FloorThickness;
				cp.Z -= CalcThickness(MOI->ObjectAssembly.Layers);
			}
		}
		else
		{
			for (auto &cp : MOI->ControlPoints)
			{
				//cp.Z += FloorThickness;
				cp.Z += CalcThickness(MOI->ObjectAssembly.Layers);
			}
		}
		UpdateDynamicGeometry();
	}

	float FMOIFlatPolyImpl::CalcThickness(const TArray<FModumateObjectAssemblyLayer> &floorLayers) const
	{
		float thickness = 0;
		for (auto &layer : floorLayers)
		{
			if (layer.Thickness.AsWorldCentimeters() > 0)
			{
				thickness += layer.Thickness.AsWorldCentimeters();
			}
		}
		return thickness;
	}


	// Dynamic Geometry
	void FMOIFlatPolyImpl::SetupDynamicGeometry()
	{
		GotGeometry = true;
		if (MOI->ObjectInverted && DynamicMeshActor->Assembly.Layers.Num() > 0)
		{
			float sketchPlaneHeight = DynamicMeshActor->GetActorLocation().Z + MOI->Extents.Y;
			float newThickness = CalcThickness(MOI->ObjectAssembly.Layers);

			TArray<FVector> newCPS = MOI->ControlPoints;
			float newZ = sketchPlaneHeight - newThickness;
			for (int32 i = 0; i < newCPS.Num(); ++i)
			{
				newCPS[i] = FVector(MOI->ControlPoints[i].X, MOI->ControlPoints[i].Y, newZ);
			}
			MOI->ControlPoints = newCPS;
		}
		DynamicMeshActor->UVFloorAnchors = MOI->ControlPoints;
		DynamicMeshActor->TopUVFloorAnchor = FVector::ZeroVector;
		DynamicMeshActor->SetupFlatPolyGeometry(MOI->ControlPoints, MOI->ObjectAssembly);
		MOI->Extents = FVector(0.0f, CalcThickness(MOI->ObjectAssembly.Layers), 0.0f);

		auto children = MOI->GetChildObjects();
		for (auto *child : children)
		{
			child->SetupGeometry();
		}
	}

	void FMOIFlatPolyImpl::UpdateDynamicGeometry()
	{
		if (!GotGeometry) return;

		DynamicMeshActor->UpdateFlatPolyGeometry(MOI->ControlPoints, MOI->ObjectAssembly);

		auto children = MOI->GetChildObjects();
		for (auto *child : children)
		{
			child->UpdateGeometry();
		}
	}

	// Adjustment Handles
	void FMOIFlatPolyImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
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
			makeActor(new FAdjustPolyPointHandle(MOI, i), AEditModelGameMode_CPP::PointAdjusterMesh, FVector(0.0007f, 0.0007f, 0.0007f), TArray<int32>{int32(i)}, 0.0f);
			makeActor(new FAdjustPolyPointHandle(MOI, i, (i + 1) % MOI->ControlPoints.Num()), AEditModelGameMode_CPP::FaceAdjusterMesh, FVector(0.0015f, 0.0015f, 0.0015f), TArray<int32>{int32(i), int32(i + 1) % MOI->ControlPoints.Num()}, 16.0f);
		}

		if (WantsInvertHandle)
		{
			makeActor(new FAdjustInvertHandle(MOI), AEditModelGameMode_CPP::InvertHandleMesh, FVector(0.003f, 0.003f, 0.003f), TArray<int32>{}, 0.0f, 2);
		}
	};

	TArray<FModelDimensionString> FMOIFlatPolyImpl::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;
		for (size_t i = 0, iend = MOI->ControlPoints.Num(); i < iend; ++i)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->ControlPoints[i];
			ds.Point2 = MOI->ControlPoints[(i + 1) % iend];
			ds.Functionality = EEnterableField::None;
			ds.Offset = 50;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ds.Owner = MOI->GetActor();
			ret.Add(ds);
		}
		return ret;
	}
}
