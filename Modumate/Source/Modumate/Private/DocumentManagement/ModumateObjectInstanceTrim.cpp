// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceTrim.h"

#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateSnappingView.h"

FMOITrimImpl::FMOITrimImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, StartAlongEdge(0.0f)
	, EndAlongEdge(1.0f)
	, EdgeStartIndex(INDEX_NONE)
	, EdgeEndIndex(INDEX_NONE)
	, EdgeMountIndex(INDEX_NONE)
	, bUseLengthAsPercent(true)
	, MiterOptionStart(ETrimMiterOptions::None)
	, MiterOptionEnd(ETrimMiterOptions::None)
	, TrimStartPos(ForceInitToZero)
	, TrimEndPos(ForceInitToZero)
	, TrimNormal(ForceInitToZero)
	, TrimUp(ForceInitToZero)
	, TrimDir(ForceInitToZero)
	, UpperExtensions(ForceInitToZero)
	, OuterExtensions(ForceInitToZero)
{
}

FMOITrimImpl::~FMOITrimImpl()
{
}

void FMOITrimImpl::SetRotation(const FQuat &r)
{
	MOI->MarkDirty(EObjectDirtyFlags::Structure);
}

FQuat FMOITrimImpl::GetRotation() const
{
	const FModumateObjectInstance *parentMOI = MOI ? MOI->GetParentObject() : nullptr;
	if (parentMOI)
	{
		return parentMOI->GetObjectRotation();
	}

	return FQuat::Identity;
}

void FMOITrimImpl::SetLocation(const FVector &p)
{
	MOI->MarkDirty(EObjectDirtyFlags::Structure);
}

FVector FMOITrimImpl::GetLocation() const
{
	if (MOI && MOI->GetControlPoints().Num() >= 2)
	{
		return 0.5f * (MOI->GetControlPoint(0) + MOI->GetControlPoint(1));
	}

	return FVector::ZeroVector;
}

AActor *FMOITrimImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	World = world;

	if (ensureAlways(world != nullptr))
	{
		AEditModelGameMode_CPP* gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
		if (gameMode)
		{
			DynamicMeshActor = world->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get(), FTransform(rot, loc));

			if (MOI && DynamicMeshActor.IsValid() && DynamicMeshActor->Mesh)
			{
				ECollisionChannel collisionObjType = UModumateTypeStatics::CollisionTypeFromObjectType(MOI->GetObjectType());
				DynamicMeshActor->Mesh->SetCollisionObjectType(collisionObjType);
			}
		}

		AEditModelGameState_CPP* gameState = world->GetAuthGameMode<AEditModelGameState_CPP>();
		if (gameState)
		{
			FModumateObjectInstance* parent = gameState->Document.GetObjectById(MOI->GetParentID());
			if (parent)
			{
				auto siblings = parent->GetChildObjects();
				for (FModumateObjectInstance* sibling : siblings)
				{
					if (sibling && (sibling->ID != MOI->ID))
					{
						sibling->UpdateGeometry();
					}
				}
			}
		}
	}

	return DynamicMeshActor.Get();
}

void FMOITrimImpl::SetupDynamicGeometry()
{
	InternalUpdateGeometry(true, true);
}

void FMOITrimImpl::UpdateDynamicGeometry()
{
	InternalUpdateGeometry(false, true);
}

void FMOITrimImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const FSimplePolygon* profile = nullptr;
	if (UModumateObjectStatics::GetPolygonProfile(&MOI->GetAssembly(), profile))
	{
		FVector2D profileSize = profile->Extents.GetSize();

		// Trims are not centered on their line start/end positions; it is their origin.
		FVector upOffset = (profileSize.X * TrimUp);
		FVector normalOffset = (profileSize.Y * TrimNormal);

		FVector centroid = 0.5f * (TrimStartPos + TrimEndPos + upOffset + normalOffset);
		FVector boxExtents(FVector::Dist(TrimStartPos, TrimEndPos), profileSize.X, profileSize.Y);
		FQuat boxRot = FRotationMatrix::MakeFromYZ(TrimUp, TrimNormal).ToQuat();

		FModumateSnappingView::GetBoundingBoxPointsAndLines(centroid, boxRot, 0.5f * boxExtents, outPoints, outLines);
	}
}

void FMOITrimImpl::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
{
	// Updated cached values for this trim
	if (!ensure(DynamicMeshActor.IsValid() && MOI && (MOI->GetAssembly().Extrusions.Num() == 1) &&
		UModumateObjectStatics::GetTrimValuesFromControls(MOI->GetControlPoints(), MOI->GetControlPointIndices(),
			StartAlongEdge, EndAlongEdge, EdgeStartIndex, EdgeEndIndex, EdgeMountIndex,
			bUseLengthAsPercent, MiterOptionStart, MiterOptionEnd)))
	{
		return;
	}

	// This can be an expected error, if the object is still getting set up before it has a parent assigned.
	const FModumateObjectInstance *parentMOI = MOI ? MOI->GetParentObject() : nullptr;
	if (parentMOI == nullptr)
	{
		return;
	}

	// Get the trim geometry, using the saved mounting index in order to
	// provide a hint about which world normal and up vectors to use.
	if (ensure(UModumateObjectStatics::GetTrimGeometryOnEdge(parentMOI, &MOI->GetAssembly(), EdgeStartIndex, EdgeEndIndex,
		StartAlongEdge, EndAlongEdge, bUseLengthAsPercent, MiterOptionStart, MiterOptionEnd,
		TrimStartPos, TrimEndPos, TrimNormal, TrimUp, EdgeMountIndex,
		UpperExtensions, OuterExtensions, FVector::ZeroVector, EdgeMountIndex, true)))
	{
		TrimDir = (TrimEndPos - TrimStartPos).GetSafeNormal();

		FVector scaleVector;
		if (!MOI->GetAssembly().TryGetProperty(BIMPropertyNames::Scale, scaleVector))
		{
			scaleVector = FVector::OneVector;
		}

		DynamicMeshActor->SetupExtrudedPolyGeometry(MOI->GetAssembly(), TrimStartPos, TrimEndPos,
			TrimNormal, TrimUp, UpperExtensions, OuterExtensions, scaleVector, bRecreate, bCreateCollision);
	}
}

void FMOITrimImpl::SetIsDynamic(bool bIsDynamic)
{
	if (DynamicMeshActor.IsValid())
	{
		DynamicMeshActor->SetIsDynamic(bIsDynamic);
	}
}

bool FMOITrimImpl::GetIsDynamic() const
{
	return DynamicMeshActor.IsValid() && DynamicMeshActor->GetIsDynamic();
}

