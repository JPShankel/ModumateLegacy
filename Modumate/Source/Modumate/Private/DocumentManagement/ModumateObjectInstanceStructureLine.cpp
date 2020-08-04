// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceStructureLine.h"

#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "DocumentManagement/ModumateSnappingView.h"

namespace Modumate
{
	FMOIStructureLine::FMOIStructureLine(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, LineStartPos(ForceInitToZero)
		, LineEndPos(ForceInitToZero)
		, LineDir(ForceInitToZero)
		, LineNormal(ForceInitToZero)
		, LineUp(ForceInitToZero)
		, UpperExtensions(ForceInitToZero)
		, OuterExtensions(ForceInitToZero)
	{
	}

	FMOIStructureLine::~FMOIStructureLine()
	{
	}

	FQuat FMOIStructureLine::GetRotation() const
	{
		FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
		if (parentObj)
		{
			return parentObj->GetObjectRotation();
		}

		return FQuat::Identity;
	}

	FVector FMOIStructureLine::GetLocation() const
	{
		FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
		if (parentObj)
		{
			return parentObj->GetObjectLocation();
		}

		return FVector::ZeroVector;
	}

	AActor *FMOIStructureLine::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;

		AEditModelGameMode_CPP *gameMode = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		if (gameMode)
		{
			DynamicMeshActor = world->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get(), FTransform(rot, loc));

			if (MOI && DynamicMeshActor.IsValid() && DynamicMeshActor->Mesh)
			{
				ECollisionChannel collisionObjType = UModumateTypeStatics::CollisionTypeFromObjectType(MOI->GetObjectType());
				DynamicMeshActor->Mesh->SetCollisionObjectType(collisionObjType);
			}
		}

		return DynamicMeshActor.Get();
	}

	void FMOIStructureLine::SetupDynamicGeometry()
	{
		InternalUpdateGeometry(true, true);
	}

	void FMOIStructureLine::UpdateDynamicGeometry()
	{
		InternalUpdateGeometry(false, true);
	}

	void FMOIStructureLine::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		const FSimplePolygon* profile = nullptr;
		if (!UModumateObjectStatics::GetPolygonProfile(&MOI->GetAssembly(), profile))
		{
			return;
		}

		// For snapping, we want to attach to the underlying line itself
		if (bForSnapping)
		{
			outPoints.Add(FStructurePoint(LineStartPos, FVector::ZeroVector, 0));
			outPoints.Add(FStructurePoint(LineEndPos, FVector::ZeroVector, 1));

			outLines.Add(FStructureLine(LineStartPos, LineEndPos, 0, 1));
		}
		// Otherwise, we want the extents of the mesh
		else
		{
			FVector2D profileSize = profile->Extents.GetSize();

			FVector2D scaleVector;
			if (MOI->GetAssembly().CachedAssembly.TryGetProperty(BIMPropertyNames::Scale, scaleVector))
			{
				profileSize *= scaleVector;
			}

			FVector centroid = 0.5f * (LineStartPos + LineEndPos);
			FVector boxExtents(FVector::Dist(LineStartPos, LineEndPos), profileSize.X, profileSize.Y);
			FQuat boxRot = FRotationMatrix::MakeFromYZ(LineUp, LineNormal).ToQuat();

			FModumateSnappingView::GetBoundingBoxPointsAndLines(centroid, boxRot, 0.5f * boxExtents, outPoints, outLines);
		}
	}

	void FMOIStructureLine::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
	{
		if (!ensure(DynamicMeshActor.IsValid() && MOI && (MOI->GetAssembly().CachedAssembly.Layers.Num() == 1)))
		{
			return;
		}

		// This can be an expected error, if the object is still getting set up before it has a parent assigned.
		const FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
		if (parentObj == nullptr)
		{
			return;
		}

		LineStartPos = parentObj->GetCorner(0);
		LineEndPos = parentObj->GetCorner(1);
		LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
		UModumateGeometryStatics::FindBasisVectors(LineNormal, LineUp, LineDir);

		FVector scaleVector;

		if (!MOI->GetAssembly().CachedAssembly.TryGetProperty(BIMPropertyNames::Scale, scaleVector))
		{
			scaleVector = FVector::OneVector;
		}

		DynamicMeshActor->SetupExtrudedPolyGeometry(MOI->GetAssembly(), LineStartPos, LineEndPos,
			LineNormal, LineUp, UpperExtensions, OuterExtensions, scaleVector, bRecreate, bCreateCollision);
	}
}
