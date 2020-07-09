// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/EdgeBase.h"

#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectStatics.h"


namespace Modumate
{
	FMOIEdgeImplBase::FMOIEdgeImplBase(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, World(nullptr)
		, LineActor(nullptr)
		, HoverColor(FColor::White)
		, HoverThickness(3.0f)
	{
		MOI->SetControlPoints(TArray<FVector>());
		MOI->AddControlPoint(FVector::ZeroVector);
		MOI->AddControlPoint(FVector::ZeroVector);
	}

	void FMOIEdgeImplBase::SetLocation(const FVector &p)
	{
		FVector midPoint = GetLocation();
		FVector delta = p - midPoint;
		MOI->SetControlPoint(0,MOI->GetControlPoint(0) + delta);
		MOI->SetControlPoint(1,MOI->GetControlPoint(1) + delta);
	}

	FVector FMOIEdgeImplBase::GetLocation() const
	{
		return 0.5f * (MOI->GetControlPoint(0) + MOI->GetControlPoint(1));
	}

	FVector FMOIEdgeImplBase::GetCorner(int32 index) const
	{
		return MOI->GetControlPoint(index);
	}

	void FMOIEdgeImplBase::OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool bEnableHover)
	{
		FModumateObjectInstanceImplBase::OnCursorHoverActor(controller, bEnableHover);

		MOI->UpdateVisibilityAndCollision();
	}

	AActor *FMOIEdgeImplBase::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;
		LineActor = world->SpawnActor<ALineActor>();
		LineActor->SetIsHUD(false);
		LineActor->UpdateMetaEdgeVisuals(false);
		return LineActor.Get();
	}

	void FMOIEdgeImplBase::SetupDynamicGeometry()
	{
		if (ensureAlways((MOI->GetControlPoints().Num() == 2) && LineActor.IsValid()))
		{
			LineActor->Point1 = MOI->GetControlPoint(0);
			LineActor->Point2 = MOI->GetControlPoint(1);
			MOI->UpdateVisibilityAndCollision();
		}
	}

	void FMOIEdgeImplBase::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIEdgeImplBase::OnSelected(bool bNewSelected)
	{
		FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

		MOI->UpdateVisibilityAndCollision();
	}

	void FMOIEdgeImplBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		// Don't report edge points for snapping, otherwise they will conflict with vertices
		if (!bForSnapping)
		{
			FVector edgeDir = (MOI->GetControlPoint(1) - MOI->GetControlPoint(0)).GetSafeNormal();

			outPoints.Add(FStructurePoint(MOI->GetControlPoint(0), edgeDir, 0));
			outPoints.Add(FStructurePoint(MOI->GetControlPoint(1), edgeDir, 1));
		}

		outLines.Add(FStructureLine(MOI->GetControlPoint(0), MOI->GetControlPoint(1), 0, 1));
	}

	float FMOIEdgeImplBase::GetThicknessMultiplier() const
	{
		return (MOI && MOI->IsSelected()) ? 3.0f : 1.0f;
	}
}
