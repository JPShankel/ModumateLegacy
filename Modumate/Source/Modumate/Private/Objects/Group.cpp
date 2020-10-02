// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Group.h"

#include "DocumentManagement/ModumateSnappingView.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/MOIGroupActor_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

float FMOIGroupImpl::StructuralExtentsExpansion = 20.0f;

FMOIGroupImpl::FMOIGroupImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, World(nullptr)
	, CachedLocation(ForceInitToZero)
	, CachedExtents(ForceInitToZero)
{ }

bool FMOIGroupImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (!FModumateObjectInstanceImplBase::CleanObject(DirtyFlag, OutSideEffectDeltas))
	{
		return false;
	}

	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		TArray<FStructurePoint> descendentPoints;
		TArray<FStructureLine> descendentLines;
		FBox groupAABB(ForceInitToZero);

		auto allChildObjs = MOI->GetAllDescendents();
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

AActor *FMOIGroupImpl::RestoreActor()
{
	if (World.IsValid())
	{
		AMOIGroupActor_CPP *groupActor = World->SpawnActor<AMOIGroupActor_CPP>(World->GetAuthGameMode<AEditModelGameMode_CPP>()->MOIGroupActorClass);
		groupActor->MOI = MOI;
		groupActor->SetActorLocation(CachedLocation);

		return groupActor;
	}

	return nullptr;
}

AActor *FMOIGroupImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	World = world;
	CachedLocation = loc;
	return RestoreActor();
}

void FMOIGroupImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	if (MOI && !CachedExtents.IsZero())
	{
		// Don't use groups for snapping
		if (bForSnapping)
		{
			return;
		}
		// For selection, use the contents of the group
		else if (bForSelection)
		{
			for (const FModumateObjectInstance *childObj : MOI->GetChildObjects())
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
