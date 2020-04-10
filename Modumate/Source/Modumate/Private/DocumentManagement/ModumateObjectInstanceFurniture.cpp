// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceFurniture.h"

#include "AdjustmentHandleActor_CPP.h"
#include "CompoundMeshActor.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "EditModelAdjustmentHandleBase.h"
#include "EditModelFFEAdjustmentHandles.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "HUDDrawWidget_CPP.h"
#include "ModumateBIMSchema.h"
#include "ModumateDocument.h"
#include "ModumateObjectInstanceParts_CPP.h"
#include "ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

class AEditModelPlayerController_CPP;

namespace Modumate
{
	FMOIObjectImpl::FMOIObjectImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, World(nullptr)
		, CachedLocation(ForceInitToZero)
		, CachedRotation(ForceInit)
		, CachedFaceNormal(ForceInitToZero)
	{}

	FMOIObjectImpl::~FMOIObjectImpl()
	{}

	void FMOIObjectImpl::InvertObject()
	{
		if (MOI->GetObjectInverted())
		{
			ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor());
			FModumateObjectAssembly newMOA = MOI->GetAssembly();
			newMOA.Layers[0].SlotScale = FVector(1.f, -1.f, 1.f);
			cma->MakeFromAssembly(newMOA, FVector::OneVector, false, true);
		}
		else
		{
			ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor());
			FModumateObjectAssembly newMOA = MOI->GetAssembly();
			newMOA.Layers[0].SlotScale = FVector(1.f, 1.f, 1.f);
			cma->MakeFromAssembly(newMOA, FVector::OneVector, false, true);
		}
	}

	AActor *FMOIObjectImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;
		return world->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
	}

	void FMOIObjectImpl::SetRotation(const FQuat &r)
	{
		FModumateObjectInstanceImplBase::SetRotation(r);

		CachedRotation = r;
	}

	FQuat FMOIObjectImpl::GetRotation() const
	{
		if (MOI->GetActor() != nullptr)
		{
			return MOI->GetActor()->GetActorQuat();
		}

		return CachedRotation;
	}

	void FMOIObjectImpl::SetLocation(const FVector &p)
	{
		FModumateObjectInstanceImplBase::SetLocation(p);

		CachedLocation = p;
	}

	FVector FMOIObjectImpl::GetLocation() const
	{
		if (MOI->GetActor() != nullptr)
		{
			return MOI->GetActor()->GetActorLocation();
		}

		return CachedLocation;
	}

	void FMOIObjectImpl::ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller)
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

	void FMOIObjectImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show)
	{
		if ((AdjustmentHandles.Num() == 0) && show)
		{
			auto makeActor = [this, controller](IAdjustmentHandleImpl *impl, UStaticMesh *mesh, const FVector &s, const TArray<int32>& CP, float offsetDist, bool upwardBillboard, const int32& side = -1)
			{
				AAdjustmentHandleActor_CPP *actor = MOI->GetActor()->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>(AAdjustmentHandleActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
				actor->SetActorMesh(mesh);
				actor->SetHandleScale(s);
				actor->SetHandleScaleScreenSize(s);
				actor->AsUpwardBillboard = upwardBillboard;
				actor->AcceptRawInputNumber = upwardBillboard;
				if (side >= 0)
				{
					actor->Side = side;
				}
				if (MOI)
				{
					actor->ParentMOI = MOI;
				}

				impl->Handle = actor;
				actor->Implementation = impl;
				actor->AttachToActor(MOI->GetActor(), FAttachmentTransformRules::KeepRelativeTransform);
				AdjustmentHandles.Add(actor);
			};

			for (int32 i = 0; i < 4; ++i)
			{
				makeActor(new FAdjustFFEPointHandle(MOI, i), AEditModelGameMode_CPP::PointAdjusterMesh, FVector(0.0007f, 0.0007f, 0.0007f), TArray<int32>{}, 0.0f, false);
			}

			makeActor(new FAdjustFFERotateHandle(MOI, 1.f), AEditModelGameMode_CPP::RotateHandleMesh, FVector(0.006f, 0.006f, 0.006f), TArray<int32>{}, 0.0f, true);
			makeActor(new FAdjustFFEInvertHandle(MOI), AEditModelGameMode_CPP::InvertHandleMesh, FVector(0.004f, 0.004f, 0.004f), TArray<int32>{}, 0.0f, false, 1);
		}
		else
		{
			for (auto &ah : AdjustmentHandles)
			{
				if (ah.IsValid())
				{
					ah->SetEnabled(show);
				}
			}
		}
	}

	void FMOIObjectImpl::SetupDynamicGeometry()
	{
		// Normalize our position on the parent wall, if necessary
		if (MOI && (MOI->GetParentID() != 0) && MOI->GetObjectType() == EObjectType::OTWallSegment)
		{
			FModumateWallMount wallMount = GetWallMountForSelf(0);
			SetWallMountForSelf(wallMount);
		}

		InternalUpdateGeometry();
	}

	void FMOIObjectImpl::UpdateDynamicGeometry()
	{
		InternalUpdateGeometry();
	}

	void FMOIObjectImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		ACompoundMeshActor *cma = MOI ? Cast<ACompoundMeshActor>(MOI->GetActor()) : nullptr;
		FVector assemblyNormal;
		TArray<FVector> boxSidePoints;

		if (cma && MOI->GetAssembly().TryGetProperty(BIM::Parameters::Normal, assemblyNormal) &&
			UModumateObjectStatics::GetFFEBoxSidePoints(cma, assemblyNormal, boxSidePoints))
		{
			// For any structure line computation, we want the points and lines projected on the plane of the actor's origin
			FVector actorLoc = cma->GetActorLocation();
			FQuat actorRot = cma->GetActorQuat();
			FVector curObjectNormal = actorRot.RotateVector(assemblyNormal);

			for (const FVector &boxSidePoint : boxSidePoints)
			{
				FVector projectedBoxPoint = FVector::PointPlaneProject(boxSidePoint, actorLoc, curObjectNormal);
				outPoints.Add(FStructurePoint(projectedBoxPoint, FVector::ZeroVector, -1));
			}

			int32 numPlanePoints = outPoints.Num();
			for (int32 pointIdx = 0; pointIdx < numPlanePoints; ++pointIdx)
			{
				const FVector &curPlanePoint = outPoints[pointIdx].Point;
				const FVector &nextPlanePoint = outPoints[(pointIdx + 1) % numPlanePoints].Point;

				outLines.Add(FStructureLine(curPlanePoint, nextPlanePoint, -1, -1));
			}

			// And for selection/bounds calculation, we also want the true bounding box
			if (!bForSnapping)
			{
				FVector cmaOrigin, cmaBoxExtent;
				cma->GetActorBounds(false, cmaOrigin, cmaBoxExtent);
				FQuat rot = GetRotation();

				// This calculates the extent more accurately since it's unaffected by actor rotation
				cmaBoxExtent = cma->CalculateComponentsBoundingBoxInLocalSpace(true).GetSize() * 0.5f;

				FModumateSnappingView::GetBoundingBoxPointsAndLines(cmaOrigin, rot, cmaBoxExtent, outPoints, outLines);
			}
		}
	}

	FModumateWallMount FMOIObjectImpl::GetWallMountForSelf(int32 originIndex) const
	{
		FModumateWallMount ret;
		ret.OriginIndex = originIndex;
		ret.RelativePosition = FVector::ZeroVector;
		ret.OrientationDelta = FQuat::Identity;

		if (MOI == nullptr)
		{
			return ret;
		}

		const FModumateObjectInstance *parent = MOI->GetParentObject();
		if (parent == nullptr)
		{
			return ret;
		}

		// Store the object's position relative to a corner of the parent that isn't expected to change.
		FVector originCornerWorldPos = parent->GetCorner(originIndex);
		FTransform wallTransform(parent->GetObjectRotation(), parent->GetObjectLocation());
		FVector originCornerRelPos = wallTransform.InverseTransformPosition(originCornerWorldPos);

		FVector objectWorldPos = MOI->GetObjectLocation();
		FVector objectRelPos = wallTransform.InverseTransformPosition(objectWorldPos);
		ret.RelativePosition = objectRelPos - originCornerRelPos;

		/*ret.OrientationDelta = MOI->GetObjectRotation() - parent->GetObjectRotation();
		ret.OrientationDelta.Normalize();*/
		ret.OrientationDelta = parent->GetObjectRotation().Inverse() * MOI->GetObjectRotation();
		ret.OriginalControlPoints = MOI->GetControlPoints();

		return ret;
	}

	void FMOIObjectImpl::SetWallMountForSelf(const FModumateWallMount &wm)
	{
		if (MOI)
		{
			FModumateObjectInstance *parent = MOI->GetParentObject();
			if (parent == nullptr || (parent->GetObjectType() != EObjectType::OTWallSegment) &&
				(parent->GetObjectType() != EObjectType::OTFloorSegment))
			{
				return;
			}

			// Use the stored position that was relative to the corner that wasn't supposed to change
			// to set the new position for the wall mounted object.
			FVector originCornerWorldPos = parent->GetCorner(wm.OriginIndex);
			FTransform wallTransform(parent->GetObjectRotation(), parent->GetObjectLocation());
			FVector originCornerRelPos = wallTransform.InverseTransformPosition(originCornerWorldPos);

			FVector mountRelativePos = wm.RelativePosition;
			// If we're mounted to the face of an object, see if that face has a finish;
			// if so, then offset this object by that finish's thickness.
			// TODO: fix for generalized plane-hosted assemblies that aren't necessarily vertical or horizontal
#if 0
			if (MOI->ControlIndices.Num() == 1)
			{
				int32 parentFaceIdx = MOI->ControlIndices[0];

				// TODO: handle mounting to faces other than a wall's left or right sides;
				// this requires computing the location relative to the face, not just the actor.
				if ((parentFaceIdx < 2) && UModumateObjectStatics::GetGeometryFromFaceIndex(
					parent, parentFaceIdx, CachedFaceIndices, CachedFaceNormal))
				{
					float parentThickness = parent->CalculateThickness();
					if (parent->ObjectType == EObjectType::OTWallSegment)
					{
						mountRelativePos.X = (parentFaceIdx == 0) ? parentThickness : 0.0f;
					}
					else if (parent->ObjectType == EObjectType::OTFloorSegment)
					{
						mountRelativePos.Z = (parentFaceIdx == 0) ? parentThickness : 0.0f;
					}

					for (int32 siblingID : parent->GetChildIDs())
					{
						const FModumateObjectInstance *sibling = MOI->GetDocument()->GetObjectById(siblingID);
						if ((sibling != MOI) &&
							(UModumateObjectStatics::GetFaceIndexFromFinishObj(sibling) == parentFaceIdx))
						{
							float siblingThickness = sibling->CalculateThickness();

							if (parent->ObjectType == EObjectType::OTWallSegment)
							{
								mountRelativePos.X += (parentFaceIdx == 0) ? siblingThickness : -siblingThickness;
							}
							else if (parent->ObjectType == EObjectType::OTFloorSegment)
							{
								mountRelativePos.Z += (parentFaceIdx == 0) ? siblingThickness : -siblingThickness;
							}

							break;
						}
					}
				}
			}
#endif

			FVector objectRelPos = mountRelativePos + originCornerRelPos;
			FVector objectWorldPos = wallTransform.TransformPosition(objectRelPos);

			MOI->SetObjectLocation(objectWorldPos);
			MOI->SetObjectRotation(parent->GetObjectRotation() * wm.OrientationDelta);

			InternalUpdateGeometry();
		}
	}

	void FMOIObjectImpl::InternalUpdateGeometry()
	{
		ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor());
		cma->MakeFromAssembly(MOI->GetAssembly(), FVector::OneVector, MOI->GetObjectInverted(), true);
	}
}
