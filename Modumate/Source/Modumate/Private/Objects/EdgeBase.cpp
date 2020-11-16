// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeBase.h"

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
	, SelectedColor(0x00, 0x35, 0xFF)
	, HoveredColor(0x00, 0x00, 0x00)
	, BaseColor(0x00, 0x00, 0x00)
	, HoverThickness(2.0f)
	, SelectedThickness(3.0f)
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

void FMOIEdgeImplBase::OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered)
{
	FModumateObjectInstanceImplBase::OnHovered(controller, bIsHovered);

	MOI->UpdateVisibilityAndCollision();
}

AActor *FMOIEdgeImplBase::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	World = world;
	LineActor = world->SpawnActor<ALineActor>();
	LineActor->SetIsHUD(false);
	LineActor->UpdateVisuals(false);
	return LineActor.Get();
}

void FMOIEdgeImplBase::OnSelected(bool bIsSelected)
{
	FModumateObjectInstanceImplBase::OnSelected(bIsSelected);

	MOI->UpdateVisibilityAndCollision();
}

void FMOIEdgeImplBase::UpdateVisibilityAndCollision(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	if (MOI && LineActor.IsValid())
	{
		UModumateObjectStatics::GetNonPhysicalEnabledFlags(MOI, bOutVisible, bOutCollisionEnabled);

		LineActor->SetVisibilityInApp(bOutVisible);
		LineActor->SetActorEnableCollision(bOutCollisionEnabled);

		if (bOutVisible)
		{
			UpdateMaterial();
		}
	}
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
	if (MOI->IsSelected())
	{
		return SelectedThickness;
	}
	else if (MOI->IsHovered())
	{
		return HoverThickness;
	}

	return 1.0f;
}

void FMOIEdgeImplBase::UpdateMaterial()
{
	if (MOI && LineActor.IsValid())
	{
		AEditModelGameMode_CPP* gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		// Color
		if (gameMode)
		{

			FColor color;
			if (MOI->IsSelected())
			{
				color = SelectedColor;
			}
			else if (MOI->IsHovered())
			{
				color = HoveredColor;
			}
			else
			{
				color = BaseColor;
			}

			LineActor->UpdateVisuals(true, GetThicknessMultiplier(), color);
		}
	}
}
