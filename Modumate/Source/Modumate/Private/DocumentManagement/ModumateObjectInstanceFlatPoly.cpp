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
		int32 numCP = MOI->GetControlPoints().Num();
		FVector extrusionDir = FVector::UpVector;

		FPlane plane;
		UModumateGeometryStatics::GetPlaneFromPoints(MOI->GetControlPoints(), plane);

		// For now, make sure that the plane of the flat poly MOI is horizontal
		if (ensureAlways((numCP >= 3) && FVector::Parallel(extrusionDir, plane)))
		{
			FVector corner = MOI->GetControlPoint(index % numCP);

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
		if (MOI->GetObjectInverted())
		{
			for (int32 i=0;i<MOI->GetControlPoints().Num();++i)
			{
				FVector cp = MOI->GetControlPoint(i);
				//cp.Z -= FloorThickness;
				cp.Z -= CalcThickness(MOI->GetAssembly().Layers);
				MOI->SetControlPoint(i, cp);
			}
		}
		else
		{
			for (int32 i = 0; i < MOI->GetControlPoints().Num(); ++i)
			{
				FVector cp = MOI->GetControlPoint(i);
				//cp.Z += FloorThickness;
				cp.Z += CalcThickness(MOI->GetAssembly().Layers);
				MOI->SetControlPoint(i, cp);
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
		if (MOI->GetObjectInverted() && DynamicMeshActor->Assembly.Layers.Num() > 0)
		{
			float sketchPlaneHeight = DynamicMeshActor->GetActorLocation().Z + MOI->GetExtents().Y;
			float newThickness = CalcThickness(MOI->GetAssembly().Layers);

			TArray<FVector> newCPS = MOI->GetControlPoints();
			float newZ = sketchPlaneHeight - newThickness;
			for (int32 i = 0; i < newCPS.Num(); ++i)
			{
				newCPS[i] = FVector(MOI->GetControlPoint(i).X, MOI->GetControlPoint(i).Y, newZ);
			}
			MOI->SetControlPoints(newCPS);
		}
		DynamicMeshActor->UVFloorAnchors = MOI->GetControlPoints();
		DynamicMeshActor->TopUVFloorAnchor = FVector::ZeroVector;
		DynamicMeshActor->SetupFlatPolyGeometry(MOI->GetControlPoints(), MOI->GetAssembly());
		MOI->SetExtents(FVector(0.0f, CalcThickness(MOI->GetAssembly().Layers), 0.0f));

		auto children = MOI->GetChildObjects();
		for (auto *child : children)
		{
			child->SetupGeometry();
		}
	}

	void FMOIFlatPolyImpl::UpdateDynamicGeometry()
	{
		if (!GotGeometry) return;

		DynamicMeshActor->UpdateFlatPolyGeometry(MOI->GetControlPoints(), MOI->GetAssembly());

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

		UStaticMesh *pointAdjusterMesh = controller->EMPlayerState->GetEditModelGameMode()->PointAdjusterMesh;
		UStaticMesh *faceAdjusterMesh = controller->EMPlayerState->GetEditModelGameMode()->FaceAdjusterMesh;
		UStaticMesh *invertHandleMesh = controller->EMPlayerState->GetEditModelGameMode()->InvertHandleMesh;

		for (size_t i = 0; i < MOI->GetControlPoints().Num(); ++i)
		{
			makeActor(new FAdjustPolyPointHandle(MOI, i), pointAdjusterMesh, FVector(0.0007f, 0.0007f, 0.0007f), TArray<int32>{int32(i)}, 0.0f);
			makeActor(new FAdjustPolyPointHandle(MOI, i, (i + 1) % MOI->GetControlPoints().Num()), faceAdjusterMesh, FVector(0.0015f, 0.0015f, 0.0015f), TArray<int32>{int32(i), int32(i + 1) % MOI->GetControlPoints().Num()}, 16.0f);
		}

		if (WantsInvertHandle)
		{
			makeActor(new FAdjustInvertHandle(MOI), invertHandleMesh, FVector(0.003f, 0.003f, 0.003f), TArray<int32>{}, 0.0f, 2);
		}
	};

	TArray<FModelDimensionString> FMOIFlatPolyImpl::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;
		for (size_t i = 0, iend = MOI->GetControlPoints().Num(); i < iend; ++i)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->GetControlPoint(i);
			ds.Point2 = MOI->GetControlPoint((i + 1) % iend);
			ds.Functionality = EEnterableField::None;
			ds.Offset = 50;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ds.Owner = MOI->GetActor();
			ret.Add(ds);
		}
		return ret;
	}
}
