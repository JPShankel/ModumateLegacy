// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceFinish.h"

#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/HUDDrawWidget_CPP.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateObjectInstance.h"


namespace Modumate
{
	FMOIFinishImpl::FMOIFinishImpl(FModumateObjectInstance *moi)
		: FDynamicModumateObjectInstanceImpl(moi)
		, CachedNormal(ForceInitToZero)
	{ }

	FMOIFinishImpl::~FMOIFinishImpl()
	{
	}

	FVector FMOIFinishImpl::GetCorner(int32 index) const
	{
		const FModumateObjectInstance *parentObj = MOI->GetParentObject();
		if (!ensureMsgf(parentObj, TEXT("Finish ID %d does not have parent object!"), MOI->ID))
		{
			return GetLocation();
		}

		float thickness = MOI->CalculateThickness();
		int32 numCP = MOI->GetControlPoints().Num();
		FVector cornerOffset = (index < numCP) ? FVector::ZeroVector : (CachedNormal * thickness);

		return MOI->GetControlPoint(index % numCP) + cornerOffset;
	}

	FVector FMOIFinishImpl::GetNormal() const
	{
		return CachedNormal;
	}

	bool FMOIFinishImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
			// For now, finishes are entirely defined by their mount to their parent wall,
			// and they inherit any bored holes, so we can just leave their setup to Visual cleaning and dirty it here.
			MOI->MarkDirty(EObjectDirtyFlags::Visuals);
			break;
		case EObjectDirtyFlags::Visuals:
			SetupDynamicGeometry();
			MOI->UpdateVisibilityAndCollision();
			break;
		default:
			break;
		}

		return true;
	}

	void FMOIFinishImpl::SetupDynamicGeometry()
	{
		// Clear adjustment handle without clearing the object tag
		for (auto &ah : AdjustmentHandles)
		{
			if (ah.IsValid())
			{
				ah->Destroy();
			}
		}
		AdjustmentHandles.Empty();

		int32 faceIndex = UModumateObjectStatics::GetFaceIndexFromFinishObj(MOI);
		auto *hostObj = MOI->GetParentObject();

		if (hostObj && (faceIndex != INDEX_NONE))
		{
			MOI->SetControlPoints(TArray<FVector>());
			CachedNormal = FVector::ZeroVector;
			float finishThickness = MOI->CalculateThickness();

			switch (hostObj->GetObjectType())
			{
			case EObjectType::OTRoofFace:
			case EObjectType::OTWallSegment:
			case EObjectType::OTFloorSegment:
			{
				const auto *hostObjParent = hostObj->GetParentObject();
				if (!(hostObjParent && (hostObjParent->GetObjectType() == EObjectType::OTMetaPlane)))
				{
					return;
				}

				int32 numPoints = hostObjParent->GetControlPoints().Num();
				float hostObjThickness = hostObj->CalculateThickness();
				FVector hostNormal = hostObj->GetNormal();

				// hostNormal can be zero if its parent hasn't been created yet;
				// otherwise, it better actually be normalized.
				if (hostNormal.IsZero() || !ensure(hostNormal.IsNormalized()))
				{
					return;
				}

				// front or back of polygon
				if (faceIndex < 2)
				{
					bool bOnFront = (faceIndex == 0);
					int32 cornerOffset = bOnFront ? numPoints : 0;

					for (int32 pointIdx = 0; pointIdx < numPoints; ++pointIdx)
					{
						int32 cornerIdx = pointIdx + cornerOffset;
						MOI->AddControlPoint(hostObj->GetCorner(cornerIdx));
					}

					CachedNormal = hostNormal * (bOnFront ? 1.0f : -1.0f);
				}
				// side of polygon
				else if (faceIndex < (numPoints + 2))
				{
					int32 startCornerIdx = faceIndex - 2;
					int32 endCornerIdx = (startCornerIdx + 1) % numPoints;
					FVector point1A = hostObj->GetCorner(startCornerIdx);
					FVector point2A = hostObj->GetCorner(endCornerIdx);
					FVector point1B = hostObj->GetCorner(startCornerIdx + numPoints);
					FVector point2B = hostObj->GetCorner(endCornerIdx + numPoints);
					FVector edgeDir = (point2A - point1A).GetSafeNormal();
					CachedNormal = edgeDir ^ hostNormal;

					MOI->SetControlPoints({ point1A, point2A, point2B, point1B });
				}
			}
			break;
			default:
				break;
			}

			ADynamicMeshActor *parentMeshActor = Cast<ADynamicMeshActor>(hostObj->GetActor());
			if ((MOI->GetControlPoints().Num() >= 3) && CachedNormal.IsNormalized() && parentMeshActor)
			{
				DynamicMeshActor->HoleActors = parentMeshActor->HoleActors;
				bool bToleratePlanarErrors = true;
				bool bLayerSetupSuccess = DynamicMeshActor->CreateBasicLayerDefs(MOI->GetControlPoints(), CachedNormal, MOI->GetAssembly(),
					0.0f, FVector::ZeroVector, 0.0f, bToleratePlanarErrors);

				if (bLayerSetupSuccess)
				{
					DynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);
				}

				auto *controller = DynamicMeshActor->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
				AEditModelPlayerState_CPP *playerState = controller ? controller->EMPlayerState : nullptr;
				if (playerState)
				{
					// Ensure that the player state's object errors are the same as this mesh's placement errors
					// TODO: combine these error collections, there's no good reason to have both of them.
					playerState->ClearErrorsForObject(MOI->ID);
					if (DynamicMeshActor->HasPlacementError())
					{
						for (const FName &placementError : DynamicMeshActor->GetPlacementErrors())
						{
							playerState->SetErrorForObject(MOI->ID, placementError, true);
						}
					}
				}
			}
		}
	}

	void FMOIFinishImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIFinishImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		int32 numCP = MOI->GetControlPoints().Num();

		for (int32 i = 0; i < numCP; ++i)
		{
			int32 edgeIdxA = i;
			int32 edgeIdxB = (i + 1) % numCP;

			FVector cornerMinA = GetCorner(edgeIdxA);
			FVector cornerMinB = GetCorner(edgeIdxB);
			FVector edgeDir = (cornerMinB - cornerMinA).GetSafeNormal();

			FVector cornerMaxA = GetCorner(edgeIdxA + numCP);
			FVector cornerMaxB = GetCorner(edgeIdxB + numCP);

			outPoints.Add(FStructurePoint(cornerMinA, edgeDir, edgeIdxA));

			outLines.Add(FStructureLine(cornerMinA, cornerMinB, edgeIdxA, edgeIdxB));
			outLines.Add(FStructureLine(cornerMaxA, cornerMaxB, edgeIdxA + numCP, edgeIdxB + numCP));
			outLines.Add(FStructureLine(cornerMinA, cornerMaxA, edgeIdxA, edgeIdxA + numCP));
		}
	}

	FModumateWallMount FMOIFinishImpl::GetWallMountForSelf(int32 originIndex) const
	{
		// TODO: add more details once finish regions exist
		return FModumateWallMount();
	}

	void FMOIFinishImpl::SetWallMountForSelf(const FModumateWallMount &wm)
	{
		SetupDynamicGeometry();
	}
}
