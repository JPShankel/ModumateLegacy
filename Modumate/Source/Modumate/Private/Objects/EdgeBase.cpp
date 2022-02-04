// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeBase.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

AMOIEdgeBase::AMOIEdgeBase()
	: AModumateObjectInstance()
	, LineActor(nullptr)
	, SelectedColor(0x03, 0x84, 0x71)
	, HoveredColor(0x00, 0x00, 0x00)
	, BaseColor(0x00, 0x00, 0x00)
	, HoverThickness(2.0f)
	, SelectedThickness(3.0f)
	, LineArrowCompNormalLength(91.44f) // 3ft
	, LineArrowCompRatio(0.33f)
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

	// Create arrow component for certain object type only
	if (UModumateTypeStatics::GetObjectTypeWithDirectionIndicator().Contains(GetObjectType()))
	{
		if (!LineArrowComponent.IsValid())
		{
			LineArrowComponent = NewObject<UArrowComponent>(this);
			LineArrowComponent->RegisterComponent();
			LineArrowComponent->SetHiddenInGame(false);
			LineArrowComponent->ArrowLength = LineArrowCompNormalLength;
			LineArrowComponent->SetArrowColor(SelectedColor);
		}
	}

	return LineActor.Get();
}

bool AMOIEdgeBase::OnSelected(bool bIsSelected)
{
	CacheIsSelected = bIsSelected;
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

	UpdateLineArrowVisual();

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

void AMOIEdgeBase::PreDestroy()
{
	if (LineArrowComponent.IsValid())
	{
		LineArrowComponent->DestroyComponent();
	}
	Super::PreDestroy();
}

void AMOIEdgeBase::UpdateLineArrowVisual()
{
	if (!LineArrowComponent.IsValid())
	{
		return;
	}
	
	bool bShowDir = CacheIsSelected;
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (controller && controller->GetAlwaysShowGraphDirection())
	{
		bShowDir = bVisible;
	}

	LineArrowComponent->SetVisibility(bShowDir);
	if (bShowDir)
	{
		LineArrowComponent->SetWorldRotation(FRotationMatrix::MakeFromX((GetCorner(1) - GetCorner(0)).GetSafeNormal()).ToQuat());
		float edgeLength = (GetCorner(1) - GetCorner(0)).Size();
		float desiredArrowLength = edgeLength * LineArrowCompRatio;
		LineArrowComponent->ArrowLength = FMath::Min(desiredArrowLength, LineArrowCompNormalLength); // Make sure length doesnt go below normal

		// Calculate arrow location
		float halfLength = (edgeLength - LineArrowComponent->ArrowLength) * 0.5f;
		float pct = halfLength / edgeLength;
		LineArrowComponent->SetWorldLocation(GetCorner(0) + pct * (GetCorner(1) - GetCorner(0)));
	}
	LineArrowComponent->SetArrowColor(CacheIsSelected ? FColor::Black : SelectedColor);
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
		auto* gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
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
