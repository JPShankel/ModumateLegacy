// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCutPlaneTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/CutPlane.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"



UCutPlaneTool::UCutPlaneTool()
	: Super()
{
}

bool UCutPlaneTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

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

	if (GameMode)
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

	AEditModelGameState *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;

	FBox bounds = doc->CalculateProjectBounds().GetBox();
	bounds = bounds.ExpandBy(DefaultPlaneDimension / 2.0f);

	// Size the cut plane to cover entire scene.
	FVector BasisX, BasisY;
	FBox2D slice(ForceInit);
	UModumateGeometryStatics::FindBasisVectors(BasisX, BasisY, Normal);

	TArray<FVector> sceneExtentBox =
	{
		FVector(bounds.Min.X, bounds.Min.Y, bounds.Min.Z),
		FVector(bounds.Min.X, bounds.Min.Y, bounds.Max.Z),
		FVector(bounds.Min.X, bounds.Max.Y, bounds.Min.Z),
		FVector(bounds.Min.X, bounds.Max.Y, bounds.Max.Z),
		FVector(bounds.Max.X, bounds.Min.Y, bounds.Min.Z),
		FVector(bounds.Max.X, bounds.Min.Y, bounds.Max.Z),
		FVector(bounds.Max.X, bounds.Max.Y, bounds.Min.Z),
		FVector(bounds.Max.X, bounds.Max.Y, bounds.Max.Z)
	};

	for (auto& p: sceneExtentBox)
	{
		slice += UModumateGeometryStatics::ProjectPoint2D(p, BasisX, BasisY, Origin);
	}

	PendingPlanePoints = {
		Origin + BasisX * slice.Min.X + BasisY * slice.Min.Y,
		Origin + BasisX * slice.Max.X + BasisY * slice.Min.Y,
		Origin + BasisX * slice.Max.X + BasisY * slice.Max.Y,
		Origin + BasisX * slice.Min.X + BasisY * slice.Max.Y
	};

	FMOICutPlaneData cutPlaneData;
	cutPlaneData.Extents = slice.GetSize();
	cutPlaneData.Location = PendingPlanePoints[0] + cutPlaneData.Extents.X * BasisX * 0.5f + cutPlaneData.Extents.Y * BasisY * 0.5f;
	cutPlaneData.Rotation = FRotationMatrix::MakeFromXY(BasisX, BasisY).ToQuat();
	cutPlaneData.Name = GetNextName();
	cutPlaneData.bIsExported = true;

	FMOIStateData stateData(doc->GetNextAvailableID(), EObjectType::OTCutPlane);
	stateData.CustomData.SaveStructData(cutPlaneData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

	bool bSuccess = doc->ApplyDeltas({ delta }, GetWorld());
	if (bSuccess)
	{
		RecentCreatedCutPlaneID = stateData.ID;
	}
	// Return false so that EndUse is called, returning true would chain cut plane creation
	return false;
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
	Controller->SetCurrentCullingCutPlane(RecentCreatedCutPlaneID);

	return UEditModelToolBase::EndUse();
}

bool UCutPlaneTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}

FString UCutPlaneTool::GetNextName() const
{
	static const TCHAR namePattern[] = TEXT("CutPlane %d");
	int32 n = 1;
	const UModumateDocument* doc = GameState->Document;

	auto existingCutPlanes = doc->GetObjectsOfType(EObjectType::OTCutPlane);
	TArray<FString> existingNames;
	for (const auto* cutPlane: existingCutPlanes)
	{
		FMOIStateData stateData = cutPlane->GetStateData();
		FMOICutPlaneData cutPlaneData;
		stateData.CustomData.LoadStructData(cutPlaneData);
		existingNames.Add(cutPlaneData.Name);
	}

	FString candidate;
	do
	{
		candidate = FString::Printf(namePattern, n++);
	} while (existingNames.Contains(candidate));

	return candidate;
}
