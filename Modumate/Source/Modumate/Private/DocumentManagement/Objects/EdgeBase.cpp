// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/EdgeBase.h"

#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectStatics.h"

FMOIEdgeImplBase::FMOIEdgeImplBase(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, World(nullptr)
	, LineActor(nullptr)
	, HoverColor(FColor::White)
	, HoverThickness(3.0f)
{
}

void FMOIEdgeImplBase::SetLocation(const FVector &p)
{
}

FVector FMOIEdgeImplBase::GetLocation() const
{
	return 0.5f * (GetCorner(0) + GetCorner(1));
}

FVector FMOIEdgeImplBase::GetCorner(int32 index) const
{
	return LineActor.IsValid() ? ((index == 0) ? LineActor->Point1 : LineActor->Point2) : FVector::ZeroVector;
}

int32 FMOIEdgeImplBase::GetNumCorners() const
{
	return 2;
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

void FMOIEdgeImplBase::OnSelected(bool bNewSelected)
{
	FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

	MOI->UpdateVisibilityAndCollision();
}

void FMOIEdgeImplBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	FVector startPoint = GetCorner(0);
	FVector endPoint = GetCorner(1);
	if (startPoint.Equals(endPoint))
	{
		return;
	}

	// Don't report edge points for snapping, otherwise they will conflict with vertices
	if (!bForSnapping)
	{
		FVector edgeDir = (endPoint - startPoint).GetSafeNormal();

		outPoints.Add(FStructurePoint(startPoint, edgeDir, 0));
		outPoints.Add(FStructurePoint(endPoint, edgeDir, 1));
	}

	outLines.Add(FStructureLine(startPoint, endPoint, 0, 1));
}

float FMOIEdgeImplBase::GetThicknessMultiplier() const
{
	return (MOI && MOI->IsSelected()) ? 3.0f : 1.0f;
}
