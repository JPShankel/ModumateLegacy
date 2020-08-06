// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelTrimTool.h"

#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerPawn_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateGeometryStatics.h"

using namespace Modumate;

UTrimTool::UTrimTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bInverted(false)
	, CurrentTarget(nullptr)
	, CurrentHitActor(nullptr)
	, CurrentStartIndex(INDEX_NONE)
	, CurrentEndIndex(INDEX_NONE)
	, CurrentMountIndex(INDEX_NONE)
	, CurrentStartAlongEdge(0.0f)
	, CurrentEndAlongEdge(0.0f)
	, bCurrentLengthsArePCT(false)
	, CurrentEdgeStart(ForceInitToZero)
	, CurrentEdgeEnd(ForceInitToZero)
	, MiterOptionStart(ETrimMiterOptions::None)
	, MiterOptionEnd(ETrimMiterOptions::None)
	, OriginalMouseMode(EMouseMode::Object)
	, TrimAssembly()
	, PendingTrimActor(nullptr)
{}

void UTrimTool::ResetTarget()
{
	bInverted = false;
	CurrentTarget = nullptr;
	CurrentTargetChildren.Reset();
	CurrentHitActor.Reset();
	CurrentStartIndex = CurrentEndIndex = CurrentMountIndex = INDEX_NONE;
	CurrentStartAlongEdge = CurrentEndAlongEdge = 0.0f;
	bCurrentLengthsArePCT = false;
	CurrentEdgeStart = CurrentEdgeEnd = FVector::ZeroVector;
}

bool UTrimTool::HasValidTarget() const
{
	return CurrentTarget && CurrentHitActor.IsValid() &&
		(CurrentStartIndex >= 0) && (CurrentEndIndex >= 0) &&
		(CurrentStartIndex != CurrentEndIndex) &&
		(CurrentStartAlongEdge < CurrentEndAlongEdge) &&
		!CurrentEdgeStart.Equals(CurrentEdgeEnd);
}

void UTrimTool::OnAssemblySet()
{
	TrimAssembly = FBIMAssemblySpec();

	if (Controller)
	{
		UWorld *world = Controller->GetWorld();
		AEditModelGameState_CPP *gameState = world->GetGameState<AEditModelGameState_CPP>();
		AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();

		// TODO: fix the case when the assembly is not already set when the tool is invoked
		const FBIMAssemblySpec *assembly = gameState ? gameState->Document.PresetManager.GetAssemblyByKey(EToolMode::VE_TRIM, AssemblyKey) : nullptr;
		if (ensureAlways(gameMode) && assembly)
		{
			TrimAssembly = *assembly;

			if (!PendingTrimActor.IsValid())
			{
				PendingTrimActor = world->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get());
			}

			FVector scaleVector;
			if (!assembly->CachedAssembly.TryGetProperty(BIMPropertyNames::Scale, scaleVector))
			{
				scaleVector = FVector::OneVector;
			}

			// Set up initial geometry, so we can just update it during FrameUpdate rather than recreate its topology
			FVector initStartPoint(0, 0, 0), initEndPoint(1, 0, 0), initNormal(0, 1, 0), initUp(0, 0, 1);
			PendingTrimActor->SetupExtrudedPolyGeometry(TrimAssembly, initStartPoint, initEndPoint, initNormal, initUp,
				FVector2D::ZeroVector, FVector2D::ZeroVector, scaleVector, true, false);
		}
	}

	if (PendingTrimActor.IsValid())
	{
		PendingTrimActor->SetActorHiddenInGame(true);
		PendingTrimActor->SetActorEnableCollision(false);
	}
}

bool UTrimTool::HandleInputNumber(double n)
{
	return false;
}

bool UTrimTool::Activate()
{
	Super::Activate();

	ResetTarget();
	bInverted = false;

	if (PendingTrimActor.IsValid())
	{
		PendingTrimActor->Destroy();
		PendingTrimActor.Reset();
	}

	OnAssemblySet();

	if (Controller)
	{
		Controller->DeselectAll();
		OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	}

	return true;
}

bool UTrimTool::Deactivate()
{
	if (Controller)
	{
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	}

	ResetTarget();

	if (PendingTrimActor.IsValid())
	{
		PendingTrimActor->Destroy();
		PendingTrimActor.Reset();
	}

	return UEditModelToolBase::Deactivate();
}

bool UTrimTool::BeginUse()
{
	if (HasValidTarget())
	{
		TArray<FVector> controlPoints;
		TArray<int32> controlIndices;

		if (ensureAlways(!AssemblyKey.IsNone() && CurrentTarget &&
			UModumateObjectStatics::GetTrimControlsFromValues(CurrentStartAlongEdge, CurrentEndAlongEdge,
				CurrentStartIndex, CurrentEndIndex, CurrentMountIndex, bCurrentLengthsArePCT,
				MiterOptionStart, MiterOptionEnd, controlPoints, controlIndices)))
		{
			auto* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();

			FMOIStateData stateData;
			stateData.StateType = EMOIDeltaType::Create;
			stateData.ObjectType = EObjectType::OTTrim;
			stateData.ObjectAssemblyKey = AssemblyKey;
			stateData.ControlPoints = controlPoints;
			stateData.ControlIndices = controlIndices;
			stateData.ParentID = CurrentTarget->ID;
			stateData.bObjectInverted = bInverted;
			stateData.ObjectID = gameState->Document.GetNextAvailableID();

			TArray<TSharedPtr<FDelta>> deltas;
			deltas.Add(MakeShareable(new FMOIDelta({ stateData })));

			gameState->Document.ApplyDeltas(deltas, GetWorld());

			EndUse();
		}
		else
		{
			AbortUse();
		}
	}
	return false;
}

bool UTrimTool::FrameUpdate()
{
	Super::FrameUpdate();

	// Reset some of the targeting variables, so we can find a new target each frame if necessary.
	FModumateObjectInstance *lastTarget = CurrentTarget;
	AActor* lastHitActor = CurrentHitActor.Get();
	CurrentTarget = nullptr;
	CurrentHitActor.Reset();
	CurrentStartIndex = CurrentEndIndex = CurrentMountIndex = INDEX_NONE;
	CurrentStartAlongEdge = CurrentEndAlongEdge = 0.0f;
	bCurrentLengthsArePCT = false;
	CurrentEdgeStart = CurrentEdgeEnd = FVector::ZeroVector;

	FVector trimStartPoint(ForceInitToZero), trimEndPoint(ForceInitToZero),
		trimNormal(ForceInitToZero), trimUp(ForceInitToZero);
	FVector2D trimUpperExtensions(ForceInitToZero), trimOuterExtensions(ForceInitToZero);

	if (Controller)
	{
		const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
		auto *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();

		if (gameState && cursor.Visible && cursor.Actor)
		{
			if (cursor.Actor == lastHitActor)
			{
				CurrentHitActor = lastHitActor;
				CurrentTarget = lastTarget;
			}
			else
			{
				CurrentHitActor = cursor.Actor;
				CurrentTarget = gameState->Document.ObjectFromActor(cursor.Actor);
			}
		}

		if (CurrentTarget != lastTarget)
		{
			CurrentTargetChildren.Reset();
			if (CurrentTarget)
			{
				CurrentTargetChildren = CurrentTarget->GetChildObjects();
			}
		}

		if (CurrentTarget && (cursor.CP1 >= 0) && (cursor.CP2 >= 0) &&
			((cursor.SnapType == ESnapType::CT_EDGESNAP) || (cursor.SnapType == ESnapType::CT_MIDSNAP)))
		{
			// A normal vector to compare against the face normals, to choose which face the trim should go on if it's unclear.
			// If we've hit a face, then choose which face to use based on that.
			FVector choiceNormal = cursor.HitNormal;
			if (!cursor.HitNormal.IsUnit())
			{
				// Otherwise, use whichever is closer to the user.
				choiceNormal = (Controller->EMPlayerPawn->CameraComponent->GetComponentLocation() - cursor.WorldPosition).GetSafeNormal();
			}

			if (UModumateObjectStatics::GetTrimGeometryOnEdge(CurrentTarget, &TrimAssembly, cursor.CP1, cursor.CP2,
				0.0f, 1.0f, true, MiterOptionStart, MiterOptionEnd, CurrentEdgeStart, CurrentEdgeEnd,
				trimNormal, trimUp, CurrentMountIndex, trimUpperExtensions, trimOuterExtensions,
				choiceNormal, INDEX_NONE, true))
			{
				FVector edgeDelta = CurrentEdgeEnd - CurrentEdgeStart;
				float edgeLength = edgeDelta.Size();
				FVector edgeDir = edgeDelta / edgeLength;
				float targetPosAlongEdge = (cursor.WorldPosition - CurrentEdgeStart) | edgeDir;
				bool bTargetIsSolidEdge = true;

				// use percent lengths on cabinet and portal edges, since they can be reshaped without preserving relative mounting info
				switch (CurrentTarget->GetObjectType())
				{
				case EObjectType::OTCabinet:
				case EObjectType::OTDoor:
				case EObjectType::OTWindow:
				{
					bCurrentLengthsArePCT = true;
					CurrentStartAlongEdge = 0.0f;
					CurrentEndAlongEdge = 1.0f;
					break;
				}
				default:
				{
					bCurrentLengthsArePCT = false;
					CurrentStartAlongEdge = 0.0f;
					CurrentEndAlongEdge = edgeLength;
					bTargetIsSolidEdge = true;
				}
				break;
				}

				// Now that we've chosen our ideal trim location, see if there's already another child in that location
				bool bTargetHasOverlappingTrim = false;
				for (FModumateObjectInstance *targetChild : CurrentTargetChildren)
				{
					float childStartLength, childEndLength;
					int32 childIdx1, childIdx2, childMountIdx;
					bool bChildLengthAsPCT;
					ETrimMiterOptions childMiterStart, childMiterEnd;

					if (targetChild && (targetChild->GetObjectType() == EObjectType::OTTrim) &&
						UModumateObjectStatics::GetTrimValuesFromControls(targetChild->GetControlPoints(), targetChild->GetControlPointIndices(),
							childStartLength, childEndLength, childIdx1, childIdx2,
							childMountIdx, bChildLengthAsPCT, childMiterStart, childMiterEnd))
					{

						// We've found another child mounted on the same edge with the same orientation
						if ((childIdx1 == cursor.CP1) && (childIdx2 == cursor.CP2) && (childMountIdx == CurrentMountIndex))
						{
							// Our cursor intersects with the existing trim, so we can't add one here
							if ((targetPosAlongEdge >= childStartLength) && (targetPosAlongEdge <= childEndLength))
							{
								bTargetHasOverlappingTrim = true;
								break;
							}
							// Our cursor is ahead of the trim we found, so the new trim has to start after its end
							if ((targetPosAlongEdge > childEndLength) && (childEndLength > CurrentStartAlongEdge))
							{
								CurrentStartAlongEdge = childEndLength;
								trimUpperExtensions.X = trimOuterExtensions.X = 0.0f;
							}
							// Our cursor is behind the trim we found, so the new trim has to end before its start
							if ((targetPosAlongEdge < childStartLength) && (childStartLength < CurrentEndAlongEdge))
							{
								CurrentEndAlongEdge = childStartLength;
								trimUpperExtensions.Y = trimOuterExtensions.Y = 0.0f;
							}
						}
					}
				}

				if (bTargetIsSolidEdge && !bTargetHasOverlappingTrim)
				{
					CurrentStartIndex = cursor.CP1;
					CurrentEndIndex = cursor.CP2;

					// interpret the trimmed edge lengths as either world lengths or percents
					float worldStartAlongEdge = CurrentStartAlongEdge * (bCurrentLengthsArePCT ? edgeLength : 1.0f);
					float worldEndAlongEdge = CurrentEndAlongEdge * (bCurrentLengthsArePCT ? edgeLength : 1.0f);

					trimStartPoint = CurrentEdgeStart + (worldStartAlongEdge * edgeDir);
					trimEndPoint = CurrentEdgeStart + (worldEndAlongEdge * edgeDir);
				}
			}
		}
	}

	bool bHasValidTarget = HasValidTarget();

	if (PendingTrimActor.IsValid())
	{
		PendingTrimActor->SetActorHiddenInGame(!bHasValidTarget);
	}

	if (bHasValidTarget)
	{
		FAffordanceLine affordanceLine({ trimStartPoint, trimEndPoint, AffordanceLineColor, AffordanceLineInterval });
		Controller->EMPlayerState->AffordanceLines.Add(MoveTemp(affordanceLine));

		if (PendingTrimActor.IsValid())
		{
			PendingTrimActor->UpdateExtrudedPolyGeometry(TrimAssembly, trimStartPoint, trimEndPoint,
				trimNormal, trimUp, trimUpperExtensions, trimOuterExtensions, FVector::OneVector, false);
		}
	}

	return true;
}

bool UTrimTool::EndUse()
{
	ResetTarget();

	if (PendingTrimActor.IsValid())
	{
		PendingTrimActor->SetActorHiddenInGame(true);
	}

	return UEditModelToolBase::EndUse();
}

bool UTrimTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}

bool UTrimTool::HandleInvert()
{
	if (!IsActive())
	{
		return false;
	}

	bInverted = !bInverted;

	return true;
}

void UTrimTool::SetAssemblyKey(const FName &InAssemblyKey)
{
	Super::SetAssemblyKey(InAssemblyKey);
	OnAssemblySet();
}
