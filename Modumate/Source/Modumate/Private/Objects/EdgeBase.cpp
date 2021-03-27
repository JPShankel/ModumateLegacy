// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeBase.h"

#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectStatics.h"

AMOIEdgeBase::AMOIEdgeBase()
	: AModumateObjectInstance()
	, LineActor(nullptr)
	, SelectedColor(0x00, 0x35, 0xFF)
	, HoveredColor(0x00, 0x00, 0x00)
	, BaseColor(0x00, 0x00, 0x00)
	, HoverThickness(2.0f)
	, SelectedThickness(3.0f)
{
}

FVector AMOIEdgeBase::GetLocation() const
{
	return 0.5f * (GetCorner(0) + GetCorner(1));
}

FVector AMOIEdgeBase::GetCorner(int32 index) const
{
	return LineActor.IsValid() ? ((index == 0) ? LineActor->Point1 : LineActor->Point2) : FVector::ZeroVector;
}

int32 AMOIEdgeBase::GetNumCorners() const
{
	return 2;
}

bool AMOIEdgeBase::OnHovered(AEditModelPlayerController *controller, bool bIsHovered)
{
	if (!AModumateObjectInstance::OnHovered(controller, bIsHovered))
	{
		return false;
	}

	MarkDirty(EObjectDirtyFlags::Visuals);

	return true;
}

AActor *AMOIEdgeBase::CreateActor(const FVector &loc, const FQuat &rot)
{
	LineActor = GetWorld()->SpawnActor<ALineActor>();
	LineActor->SetIsHUD(false);
	LineActor->MakeGeometry();
	LineActor->UpdateLineVisuals(false);
	return LineActor.Get();
}

bool AMOIEdgeBase::OnSelected(bool bIsSelected)
{
	if (!AModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	MarkDirty(EObjectDirtyFlags::Visuals);

	return true;
}

bool AMOIEdgeBase::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	if (!LineActor.IsValid())
	{
		return false;
	}

	if (!UModumateObjectStatics::GetNonPhysicalEnabledFlags(this, bOutVisible, bOutCollisionEnabled))
	{
		return false;
	}

	LineActor->SetVisibilityInApp(bOutVisible);
	LineActor->SetActorEnableCollision(bOutCollisionEnabled);

	if (bOutVisible)
	{
		UpdateMaterial();
	}

	return true;
}

void AMOIEdgeBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
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

float AMOIEdgeBase::GetThicknessMultiplier() const
{
	if (IsSelected())
	{
		return SelectedThickness;
	}
	else if (IsHovered())
	{
		return HoverThickness;
	}

	return 1.0f;
}

void AMOIEdgeBase::UpdateMaterial()
{
	if (LineActor.IsValid())
	{
		AEditModelGameMode* gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
		// Color
		if (gameMode)
		{

			FColor color;
			if (IsSelected())
			{
				color = SelectedColor;
			}
			else if (IsHovered())
			{
				color = HoveredColor;
			}
			else
			{
				color = BaseColor;
			}

			LineActor->UpdateLineVisuals(true, GetThicknessMultiplier(), color);
		}
	}
}
