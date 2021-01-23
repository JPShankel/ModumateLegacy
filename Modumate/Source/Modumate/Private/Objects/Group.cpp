// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Group.h"

#include "DocumentManagement/ModumateSnappingView.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/MOIGroupActor.h"
#include "UnrealClasses/EditModelPlayerController.h"

float AMOIGroup::StructuralExtentsExpansion = 20.0f;

AMOIGroup::AMOIGroup()
	: AModumateObjectInstance()
	, CachedLocation(ForceInitToZero)
	, CachedExtents(ForceInitToZero)
{ }

bool AMOIGroup::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (!AModumateObjectInstance::CleanObject(DirtyFlag, OutSideEffectDeltas))
	{
		return false;
	}

	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		TArray<FStructurePoint> descendentPoints;
		TArray<FStructureLine> descendentLines;
		FBox groupAABB(ForceInitToZero);

		auto allChildObjs = GetAllDescendents();
		for (auto* childObj : allChildObjs)
		{
			descendentPoints.Reset();
			descendentLines.Reset();
			childObj->GetStructuralPointsAndLines(descendentPoints, descendentLines);
			for (auto& point : descendentPoints)
			{
				groupAABB += point.Point;
			}
		}

		CachedLocation = groupAABB.GetCenter();
		CachedExtents = groupAABB.GetSize();
	}

	return true;
}

AActor *AMOIGroup::RestoreActor()
{
	UWorld* world = GetWorld();
	auto* gameMode = world->GetAuthGameMode<AEditModelGameMode>();
	auto* groupActorClass = gameMode ? gameMode->MOIGroupActorClass : nullptr;
	if (!ensure(groupActorClass))
	{
		return nullptr;
	}

	AMOIGroupActor* groupActor = world->SpawnActor<AMOIGroupActor>(groupActorClass);
	groupActor->MOI = this;
	groupActor->SetActorLocation(CachedLocation);

	return groupActor;
}

AActor *AMOIGroup::CreateActor(const FVector &loc, const FQuat &rot)
{
	CachedLocation = loc;
	return RestoreActor();
}

void AMOIGroup::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	if (!CachedExtents.IsZero())
	{
		// Don't use groups for snapping
		if (bForSnapping)
		{
			return;
		}
		// For selection, use the contents of the group
		else if (bForSelection)
		{
			for (const AModumateObjectInstance *childObj : GetChildObjects())
			{
				childObj->GetStructuralPointsAndLines(TempPoints, TempLines, bForSnapping, bForSelection);
				outPoints.Append(TempPoints);
				outLines.Append(TempLines);
			}
		}
		// Otherwise, use the AABB to render a preview of the group
		else
		{
			FVector halfExtents = 0.5f * CachedExtents;
			halfExtents += FVector(StructuralExtentsExpansion);

			FModumateSnappingView::GetBoundingBoxPointsAndLines(CachedLocation, FQuat::Identity, halfExtents, outPoints, outLines);
		}
	}
}
