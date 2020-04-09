// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelCutPlaneTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"

#include "ModumateCommands.h"

#include "ModumateGeometryStatics.h"

using namespace Modumate;

UCutPlaneTool::UCutPlaneTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	GameMode = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
}

bool UCutPlaneTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	Controller->SetCutPlaneVisibility(true);

	return UEditModelToolBase::Activate();
}

bool UCutPlaneTool::Deactivate()
{
	Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	return UEditModelToolBase::Deactivate();
}

bool UCutPlaneTool::BeginUse()
{
	Super::BeginUse();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	Origin = hitLoc;
	Normal = (hitLoc - Origin).GetSafeNormal();

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	const FVector &tangent = Controller->EMPlayerState->SnappedCursor.HitTangent;
	const FVector &normal = Controller->EMPlayerState->SnappedCursor.HitNormal;
	if (!FMath::IsNearlyZero(normal.Size()))
	{
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, normal, tangent);
	}
	else
	{
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, tangent);
	}

	if (GameMode.IsValid())
	{
		PendingPlane = Controller->GetWorld()->SpawnActor<ADynamicMeshActor>(GameMode->DynamicMeshActorClass.Get());
		PendingPlane->SetActorHiddenInGame(true);
		PendingPlaneMaterial.EngineMaterial = GameMode->MetaPlaneMaterial;
	}

	PendingPlanePoints.Reset();

	return true;
}

bool UCutPlaneTool::FrameUpdate()
{
	Super::FrameUpdate();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (PendingPlane != nullptr)
	{
		Normal = (hitLoc - Origin).GetSafeNormal();

		FVector BasisX, BasisY;
		UModumateGeometryStatics::FindBasisVectors(BasisX, BasisY, Normal);

		FVector cutPlaneX = 0.5f * DefaultPlaneDimension * BasisX;
		FVector cutPlaneY = 0.5f * DefaultPlaneDimension * BasisY;

		PendingPlanePoints = {
			Origin - cutPlaneX - cutPlaneY,
			Origin - cutPlaneX + cutPlaneY,
			Origin + cutPlaneX + cutPlaneY,
			Origin + cutPlaneX - cutPlaneY
		};

		PendingPlane->SetActorHiddenInGame(false);
		PendingPlane->SetupPlaneGeometry(PendingPlanePoints, PendingPlaneMaterial, true, false);
	}

	return true;
}

bool UCutPlaneTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const FModumateDocument *doc = &gameState->Document;

	FBox bounds = doc->CalculateProjectBounds().GetBox();
	bounds = bounds.ExpandBy(DefaultPlaneDimension / 2.0f);

	// if the cut plane will be inside of the project bounds, attempt to size the plane to match the bounds
	FVector BasisX, BasisY;
	FBox2D slice;
	if (UModumateGeometryStatics::SliceBoxWithPlane(bounds, Origin, Normal, BasisX, BasisY, slice))
	{
		PendingPlanePoints = {
			Origin + BasisX * slice.Min.X + BasisY * slice.Min.Y,
			Origin + BasisX * slice.Max.X + BasisY * slice.Min.Y,
			Origin + BasisX * slice.Max.X + BasisY * slice.Max.Y,
			Origin + BasisX * slice.Min.X + BasisY * slice.Max.Y
		};
	}

	auto commandResult = Controller->ModumateCommand(
		FModumateCommand(Commands::kMakeCutPlane)
		.Param(Parameters::kControlPoints, PendingPlanePoints)
	);

	if (commandResult.GetValue(Parameters::kSuccess))
	{
		EndUse();
	}

	return commandResult.GetValue(Parameters::kSuccess);
}

bool UCutPlaneTool::EndUse()
{
	if (PendingPlane.IsValid())
	{
		PendingPlane->Destroy();
		PendingPlane.Reset();
	}

	PendingPlanePoints.Reset();

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return UEditModelToolBase::EndUse();
}

bool UCutPlaneTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}

