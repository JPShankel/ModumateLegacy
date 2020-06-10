// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceLineSegment.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"

namespace Modumate
{
	void FMOILineSegment::SetMaterial(UMaterialInterface *m)
	{
	}

	TArray<FModelDimensionString> FMOILineSegment::GetDimensionStrings() const
	{
		TArray<FModelDimensionString> ret;
		if (MOI->GetControlPoints().Num() > 0)
		{
			FModelDimensionString ds;
			ds.AngleDegrees = 0;
			ds.Point1 = MOI->GetControlPoint(0);
			ds.Point2 = MOI->GetControlPoint(1);
			ds.Functionality = EEnterableField::None;
			ds.Offset = 50;
			ds.UniqueID = MOI->GetActor()->GetFName();
			ds.Owner = MOI->GetActor();
			ret.Add(ds);
		}
		return ret;
	}

	UMaterialInterface *FMOILineSegment::GetMaterial()
	{
		return Material.Get();
	}

	void FMOILineSegment::SetRotation(const FQuat &r)
	{
		NormalRotation = r;
		UpdateControlPoints();
	};

	FQuat FMOILineSegment::GetRotation() const
	{
		return NormalRotation;
	}

	void FMOILineSegment::SetLocation(const FVector &p)
	{
		NormalLocation = p;
		UpdateControlPoints();
	}

	FVector FMOILineSegment::GetLocation() const
	{
		return NormalLocation;
	}

	void FMOILineSegment::ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		for (auto &ah : AdjustmentHandles)
		{
			ah->Destroy();
		}
		AdjustmentHandles.Empty();
	}

	void FMOILineSegment::ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show)
	{
		for (auto &ah : AdjustmentHandles)
		{
			if (ah.IsValid())
			{
				ah->SetEnabled(show);
			}
		}
	}

	void FMOILineSegment::GetAdjustmentHandleActors(TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>>& outHandleActors)
	{
		outHandleActors = AdjustmentHandles;
	}

	AActor *FMOILineSegment::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;
		LineActor = world->SpawnActor<ALineActor>();
		LineActor->Color = BaseColor;
		LineActor->Thickness = 2.0f;
		return LineActor.Get();
	}

	void FMOILineSegment::SetupDynamicGeometry()
	{
		if (MOI->GetControlPoints().Num() >= 2)
		{
			LineActor->Point1 = MOI->GetControlPoint(0);
			LineActor->Point2 = MOI->GetControlPoint(1);
		}
	}

	void FMOILineSegment::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOILineSegment::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		FVector dir = (MOI->GetControlPoint(1) - MOI->GetControlPoint(0)).GetSafeNormal();

		outPoints.Add(FStructurePoint(MOI->GetControlPoint(0), dir, 0));
		outPoints.Add(FStructurePoint(MOI->GetControlPoint(1), dir, 1));
		outLines.Add(FStructureLine(MOI->GetControlPoint(0), MOI->GetControlPoint(1), 0, 1));
	}
}
