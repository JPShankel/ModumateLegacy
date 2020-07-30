// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceFlatPoly.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Common/EditModelPolyAdjustmentHandles.h"
#include "UnrealClasses/LineActor.h"
#include "ModumateCore/ModumateGeometryStatics.h"

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

// This method of inverting objects was doomed to begin with; it has been outdated by plane-hosted objects,
// which countertops will almost certainly need to be (or borrow from).
#if 0
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
#endif

	float FMOIFlatPolyImpl::CalcThickness(const FBIMAssemblySpec &Assembly) const
	{
		float thickness = 0;
		for (auto &layer : Assembly.CachedAssembly.Layers)
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
		if (MOI->GetObjectInverted() && DynamicMeshActor->Assembly.CachedAssembly.Layers.Num() > 0)
		{
			float sketchPlaneHeight = DynamicMeshActor->GetActorLocation().Z + MOI->GetExtents().Y;
			float newThickness = CalcThickness(MOI->GetAssembly());

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
		MOI->SetExtents(FVector(0.0f, CalcThickness(MOI->GetAssembly()), 0.0f));

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
		// Make the polygon adjustment handles, for modifying the polygonal shape
		int32 numCP = MOI->GetControlPoints().Num();
		for (int32 i = 0; i < numCP; ++i)
		{
			auto cornerHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
			cornerHandle->SetTargetIndex(i);

			auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
			edgeHandle->SetTargetIndex(i);
			edgeHandle->SetAdjustPolyEdge(true);
		}
	}

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
