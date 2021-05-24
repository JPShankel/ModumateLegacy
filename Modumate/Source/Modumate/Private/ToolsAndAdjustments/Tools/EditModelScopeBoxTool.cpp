#include "ToolsAndAdjustments/Tools/EditModelScopeBoxTool.h"

#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

#include "DocumentManagement/ModumateCommands.h"

UScopeBoxTool::UScopeBoxTool()
	: Super()
{
}

bool UScopeBoxTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	ResetState();
		
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	CurrentState = SeekingCutPlane;

	return true;
}

bool UScopeBoxTool::Deactivate()
{
	ResetState();

	if (Controller)
	{
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	}

	return UEditModelToolBase::Deactivate();
}

bool UScopeBoxTool::BeginUse()
{
	Super::BeginUse();


	return EnterNextStage();
}

bool UScopeBoxTool::FrameUpdate()
{
	Super::FrameUpdate();

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	if (!cursor.Visible)
	{
		return false;
	}
	FVector hitLoc = cursor.WorldPosition;

	if (PendingBox != nullptr)
	{
		switch (CurrentState)
		{
		case BasePending:
		{
			FVector currentPoint = cursor.SketchPlaneProject(hitLoc);

			PendingSegment->Point2 = hitLoc;
			PendingSegment->UpdateLineVisuals(false);

			if ((currentPoint - Origin).IsNearlyZero())
			{
				break;
			}

			FVector BasisX, BasisY;
			UModumateGeometryStatics::FindBasisVectors(BasisX, BasisY, Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

			FVector offset1 = (currentPoint - Origin).ProjectOnTo(BasisX) + Origin;
			FVector offset2 = (currentPoint - Origin).ProjectOnTo(BasisY) + Origin;

			// Avoid drawing invalid rectangles
			if ((offset1 - Origin).IsNearlyZero() || 
				(offset2 - Origin).IsNearlyZero())
			{
				break;
			}

			if ((offset1 - currentPoint).IsNearlyZero() || 
				(offset2 - currentPoint).IsNearlyZero())
			{
				break;
			}

			PendingBoxBasePoints = {
				Origin,
				offset1,
				currentPoint,
				offset2
			};


			PendingBox->SetActorHiddenInGame(false);
			PendingBox->SetupPlaneGeometry(PendingBoxBasePoints, PendingBoxMaterial, true);
		}
		break;
		case NormalPending:
		{
			FVector rawDelta = hitLoc - PendingSegment->Point2;
			float thickness = rawDelta | Normal;
			if (thickness > KINDA_SMALL_NUMBER)
			{
				PendingBox->SetupPrismGeometry(PendingBoxBasePoints, thickness * FVector::UpVector, PendingBoxMaterial, false, false);

				Extrusion = thickness;
			}
		}
		break;
		}
	}

	return true;
}

bool UScopeBoxTool::EnterNextStage()
{
	Super::EnterNextStage();

	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	if (!cursor.Visible)
	{
		return false;
	}
	FVector hitLoc = cursor.WorldPosition;

	switch (CurrentState)
	{
	case SeekingCutPlane:
	{
		AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject; 
		if (newTarget != nullptr && newTarget->GetObjectType() == EObjectType::OTCutPlane)
		{
			if (GameMode == nullptr)
			{
				return false;
			}
			PendingBox = Controller->GetWorld()->SpawnActor<ADynamicMeshActor>(GameMode->DynamicMeshActorClass.Get());
			PendingBox->SetActorHiddenInGame(true);
			PendingBox->SetActorEnableCollision(false);

			PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor>();

			PendingBoxMaterial.EngineMaterial = GameMode->ScopeBoxMaterial;

			Origin = hitLoc;
			Normal = newTarget->GetNormal();
			PendingSegment->Point1 = Origin;

			cursor.SetAffordanceFrame(Origin, newTarget->GetNormal());

			CurrentState = BasePending;
		}
	}
	break;
	case NormalPending:
	{
		if (Extrusion < KINDA_SMALL_NUMBER)
		{
			EndUse();
			return false;
		}

		auto commandResult = Controller->ModumateCommand(
			FModumateCommand(ModumateCommands::kMakeScopeBox)
			.Param(ModumateParameters::kControlPoints, PendingBoxBasePoints)
			.Param(ModumateParameters::kHeight, Extrusion)
		);

		if (commandResult.GetValue(ModumateParameters::kSuccess))
		{
			EndUse();
		}

		return commandResult.GetValue(ModumateParameters::kSuccess);
	}
	break;
	case BasePending:
	{
		FPlane plane;
		UModumateGeometryStatics::GetPlaneFromPoints(PendingBoxBasePoints, plane);
		if (!FVector::Coincident(plane, Normal))
		{
			// swap winding to effectively flip normal to match
			Swap(PendingBoxBasePoints[1], PendingBoxBasePoints[3]);
		}

		FVector BasisX, BasisY;
		UModumateGeometryStatics::FindBasisVectors(BasisX, BasisY, Normal);
		cursor.WantsVerticalAffordanceSnap = true;
		cursor.SetAffordanceFrame(PendingSegment->Point2, BasisX, Normal);
		CurrentState = NormalPending;
	}
	break;
	}

	return true;
}

bool UScopeBoxTool::EndUse()
{
	if (PendingBox.IsValid())
	{
		PendingBox->Destroy();
		PendingBox.Reset();
	}

	if (PendingSegment.IsValid())
	{
		PendingSegment->Destroy();
		PendingSegment.Reset();
	}

	PendingBoxBasePoints.Reset();
	ResetState();

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return UEditModelToolBase::EndUse();
}

bool UScopeBoxTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}

bool UScopeBoxTool::ResetState()
{
	CurrentState = SeekingCutPlane;

	Origin = FVector(FVector::ZeroVector);
	Normal = FVector(FVector::ZeroVector);
	Extrusion = 0.0f;

	PendingBox.Reset();
	PendingSegment.Reset();

	PendingBoxMaterial = FArchitecturalMaterial();

	PendingBoxBasePoints.Reset();

	return true;
}

