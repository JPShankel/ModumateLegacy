#include "DocumentManagement/ModumateDynamicObjectBase.h"
#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"

namespace Modumate
{
	// Actor Management
	AActor *FDynamicModumateObjectInstanceImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;

		if (AEditModelGameMode_CPP *gameMode = World->GetAuthGameMode<AEditModelGameMode_CPP>())
		{
			DynamicMeshActor = World->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get(), FTransform(rot, loc));

			if (MOI && DynamicMeshActor.IsValid() && DynamicMeshActor->Mesh)
			{
				ECollisionChannel collisionObjType = UModumateTypeStatics::CollisionTypeFromObjectType(MOI->GetObjectType());
				DynamicMeshActor->Mesh->SetCollisionObjectType(collisionObjType);
			}
		}

		return DynamicMeshActor.Get();
	}

	// Material
	void FDynamicModumateObjectInstanceImpl::SetMaterial(UMaterialInterface *m)
	{
		DynamicMeshActor->Mesh->SetMaterial(0, m);
	}

	UMaterialInterface *FDynamicModumateObjectInstanceImpl::GetMaterial()
	{
		return DynamicMeshActor->Mesh->GetMaterial(0);
	}

	// Location/Rotation
	void FDynamicModumateObjectInstanceImpl::SetRotation(const FQuat &r)
	{
		NormalRotation = r;
		UpdateControlPoints();
	}

	FQuat FDynamicModumateObjectInstanceImpl::GetRotation() const
	{
		return NormalRotation;
	}

	void FDynamicModumateObjectInstanceImpl::SetLocation(const FVector &p)
	{
		NormalLocation = p;
		UpdateControlPoints();
	}

	FVector FDynamicModumateObjectInstanceImpl::GetLocation() const
	{
		return NormalLocation;
	}

	void FDynamicModumateObjectInstanceImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		int32 numPolyPoints = MOI->GetControlPoints().Num();

		for (int32 i = 0; i < numPolyPoints; ++i)
		{
			int32 nextI = (i + 1) % numPolyPoints;
			const FVector &cp1Bottom = MOI->GetControlPoint(i);
			const FVector &cp2Bottom = MOI->GetControlPoint(nextI);
			FVector dir = (cp2Bottom - cp1Bottom).GetSafeNormal();

			FVector heightOffset = MOI->GetExtents().Y * FVector::UpVector;
			FVector cp1Top = cp1Bottom + heightOffset;
			FVector cp2Top = cp2Bottom + heightOffset;

			outPoints.Add(FStructurePoint(cp1Bottom, dir, i));
			outPoints.Add(FStructurePoint(cp1Top, dir, i));

			outLines.Add(FStructureLine(cp1Bottom, cp2Bottom, i, nextI));
			outLines.Add(FStructureLine(cp1Bottom, cp1Top, i, i + numPolyPoints));
			outLines.Add(FStructureLine(cp1Top, cp2Top, i + numPolyPoints, nextI + numPolyPoints));
		}
	}

	bool FDynamicModumateObjectInstanceImpl::GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const
	{
		return DynamicMeshActor.IsValid() ? DynamicMeshActor->GetTriInternalNormalFromEdge(cp1, cp2, outNormal) : false;
	}

	void FDynamicModumateObjectInstanceImpl::ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller)
	{
		for (auto &ah : AdjustmentHandles)
		{
			if (ah.IsValid())
			{
				ah->Destroy();
			}
		}
		AdjustmentHandles.Empty();
	}

	void FDynamicModumateObjectInstanceImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show)
	{
		if (show)
		{
			SetupAdjustmentHandles(controller);
		}

		for (auto &ah : AdjustmentHandles)
		{
			if (ah.IsValid())
			{
				// If this handle has children, show or hide as intended
				if (ah->HandleChildren.Num() > 0)
				{
					ah->SetEnabled(show);
					// If hide, children should also hide children
					if (!show)
					{
						ah->bShowHandleChildren = show;
					}
				}
				// If this handle is a child, show only when bool show is true
				else if (ah->HandleChildren.Num() == 0 && ah->HandleParent && show == true)
				{
					ah->SetEnabled(ah->HandleParent->bShowHandleChildren);
				}
				else
				{
					ah->SetEnabled(show);
				}
			}
		}

	}

	void FDynamicModumateObjectInstanceImpl::SetIsDynamic(bool bIsDynamic)
	{
		if (DynamicMeshActor.IsValid())
		{
			DynamicMeshActor->SetIsDynamic(bIsDynamic);
		}
	}

	bool FDynamicModumateObjectInstanceImpl::GetIsDynamic() const
	{
		return DynamicMeshActor.IsValid() && DynamicMeshActor->GetIsDynamic();
	}

	void FDynamicModumateObjectInstanceImpl::GetAdjustmentHandleActors(TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>>& outHandleActors)
	{
		outHandleActors = AdjustmentHandles;
	}
}
