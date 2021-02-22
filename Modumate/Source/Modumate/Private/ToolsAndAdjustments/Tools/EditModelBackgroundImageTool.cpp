// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelBackgroundImageTool.h"

#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Objects/BackgroundImage.h"


UBackgroundImageTool::UBackgroundImageTool(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	auto* world = GetWorld();
	if (world)
	{
		GameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	}
}

bool UBackgroundImageTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	return Super::Activate();
}

bool UBackgroundImageTool::Deactivate()
{
	Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	return Super::Deactivate();
}

bool UBackgroundImageTool::BeginUse()
{
	Super::BeginUse();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	Origin = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	Normal = FVector::UpVector;

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	const FVector& tangent = Controller->EMPlayerState->SnappedCursor.HitTangent;
	const FVector& normal = Controller->EMPlayerState->SnappedCursor.HitNormal;
	if (!FMath::IsNearlyZero(normal.Size()))
	{
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(Origin, normal, tangent);
	}
	else
	{
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(Origin, FVector::UpVector, tangent);
	}

	if (GameMode)
	{
		PendingImageActor = GetWorld()->SpawnActor<ADynamicMeshActor>(GameMode->DynamicMeshActorClass.Get());
		PendingImageActor->SetActorHiddenInGame(true);
		PendingPlaneMaterial.EngineMaterial = GameMode->MetaPlaneMaterial;
	}

	return true;
}

bool UBackgroundImageTool::EndUse()
{
	if (PendingImageActor.IsValid())
	{
		PendingImageActor->Destroy();
		PendingImageActor.Reset();
	}
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	return Super::EndUse();
}

bool UBackgroundImageTool::AbortUse()
{
	EndUse();
	return Super::AbortUse();
}

bool UBackgroundImageTool::FrameUpdate()
{
	Super::FrameUpdate();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	if (PendingImageActor.IsValid())
	{
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		Normal = (hitLoc - Origin).GetSafeNormal();
		FVector planeX, planeY;
		UModumateGeometryStatics::FindBasisVectors(planeX, planeY, Normal);
		planeX *= 0.5f * InitialSize;
		planeY *= 0.5f * InitialSize;

		TArray<FVector> planePoints = { Origin - planeX - planeY,
			Origin + planeX - planeY,
			Origin + planeX + planeY,
			Origin - planeX + planeY };

		PendingImageActor->SetActorHiddenInGame(false);
		PendingImageActor->SetupPlaneGeometry(planePoints, PendingPlaneMaterial, true, false);
	}

	return true;
}

bool UBackgroundImageTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FString imageFilename;
	if (!Modumate::PlatformFunctions::GetOpenFilename(imageFilename, false) || imageFilename.IsEmpty())
	{
		return false;
	}
	if (!FPaths::FileExists(imageFilename))
	{
		return false;
	}

	auto* doc = GameMode->GetGameState<AEditModelGameState>()->Document;
	FMOIBackgroundImageData moiData;
	moiData.Location = Origin;

	FVector planeX, planeY;
	UModumateGeometryStatics::FindBasisVectors(planeX, planeY, Normal);
	moiData.Rotation = FRotationMatrix::MakeFromXY(planeX, planeY).ToQuat();
	moiData.Scale = 1.0f;
	moiData.SourceFilename = imageFilename;

	FMOIStateData stateData(doc->GetNextAvailableID(), EObjectType::OTBackgroundImage);
	stateData.CustomData.SaveStructData(moiData);
	auto createDelta = MakeShared<FMOIDelta>();
	createDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	doc->ApplyDeltas({ createDelta }, GetWorld());

	return false;
}
