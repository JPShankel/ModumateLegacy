// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectGroup.h"
#include "CoreMinimal.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "DocumentManagement/ModumateObjectGroup.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/MOIGroupActor_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/HUDDrawWidget_CPP.h"

namespace Modumate
{
	float FMOIGroupImpl::StructuralExtentsExpansion = 20.0f;

	FMOIGroupImpl::FMOIGroupImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, World(nullptr)
		, Location(ForceInitToZero)
	{ }

	void FMOIGroupImpl::SetLocation(const FVector &p)
	{
		FModumateObjectInstanceImplBase::SetLocation(p);

		Location = p;
	}

	bool FMOIGroupImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		if (!FModumateObjectInstanceImplBase::CleanObject(DirtyFlag))
		{
			return false;
		}

		TArray<FStructurePoint> descendentPoints;
		TArray<FStructureLine> descendentLines;
		FBox groupAABB(ForceInitToZero);

		auto allChildObjs = MOI->GetAllDescendents();
		for (auto *childObj : allChildObjs)
		{
			descendentPoints.Reset();
			descendentLines.Reset();
			childObj->GetStructuralPointsAndLines(descendentPoints, descendentLines);
			for (auto &point : descendentPoints)
			{
				groupAABB += point.Point;
			}
		}

		// TODO: preserve rotation if it was already set
		MOI->SetObjectLocation(groupAABB.GetCenter());
		MOI->SetObjectRotation(FQuat::Identity);
		MOI->SetExtents(groupAABB.GetSize());

		return true;
	}

	AActor *FMOIGroupImpl::RestoreActor()
	{
		if (World.IsValid())
		{			
			AMOIGroupActor_CPP *groupActor = World->SpawnActor<AMOIGroupActor_CPP>(World->GetAuthGameMode<AEditModelGameMode_CPP>()->MOIGroupActorClass);
			groupActor->MOI = MOI;
			groupActor->SetActorLocation(Location);

			return groupActor;
		}

		return nullptr;
	}

	AActor *FMOIGroupImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;
		Location = loc;
		return RestoreActor();
	}

	void FMOIGroupImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		if (MOI && !MOI->GetExtents().IsZero())
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
				FVector center = GetLocation();

				FVector halfExtents = 0.5f * MOI->GetExtents();
				halfExtents += FVector(StructuralExtentsExpansion);

				FQuat rot = GetRotation();

				FModumateSnappingView::GetBoundingBoxPointsAndLines(center, rot, halfExtents, outPoints, outLines);
			}
		}
	}
}
