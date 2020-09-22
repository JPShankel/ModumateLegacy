// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/FFE.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "ToolsAndAdjustments/Handles/AdjustFFEPointHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustFFERotateHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustFFEInvertHandle.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "BIMKernel/BIMProperties.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/ModumateObjectInstanceParts_CPP.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

class AEditModelPlayerController_CPP;

FMOIFFEImpl::FMOIFFEImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, World(nullptr)
	, CachedLocation(ForceInitToZero)
	, CachedRotation(ForceInit)
	, CachedFaceNormal(ForceInitToZero)
{}

FMOIFFEImpl::~FMOIFFEImpl()
{}

AActor *FMOIFFEImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	World = world;
	return world->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

void FMOIFFEImpl::SetRotation(const FQuat &r)
{
	FModumateObjectInstanceImplBase::SetRotation(r);

	CachedRotation = r;
}

FQuat FMOIFFEImpl::GetRotation() const
{
	if (MOI->GetActor() != nullptr)
	{
		return MOI->GetActor()->GetActorQuat();
	}

	return CachedRotation;
}

void FMOIFFEImpl::SetLocation(const FVector &p)
{
	FModumateObjectInstanceImplBase::SetLocation(p);

	CachedLocation = p;
}

FVector FMOIFFEImpl::GetLocation() const
{
	if (MOI->GetActor() != nullptr)
	{
		return MOI->GetActor()->GetActorLocation();
	}

	return CachedLocation;
}

void FMOIFFEImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	for (int32 i = 0; i < 4; ++i)
	{
		auto ffePointHandle = MOI->MakeHandle<AAdjustFFEPointHandle>();
		ffePointHandle->SetTargetIndex(i);
	}

	auto ffeRotateHandle = MOI->MakeHandle<AAdjustFFERotateHandle>();
	auto ffeInvertHandle = MOI->MakeHandle<AAdjustFFEInvertHandle>();
}

void FMOIFFEImpl::SetupDynamicGeometry()
{
	// TODO: re-implement wall-mounted FF&E, either with optional surface graphs meta vertex mounts
	InternalUpdateGeometry();
}

void FMOIFFEImpl::UpdateDynamicGeometry()
{
	InternalUpdateGeometry();
}

void FMOIFFEImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	ACompoundMeshActor *cma = MOI ? Cast<ACompoundMeshActor>(MOI->GetActor()) : nullptr;
	FVector assemblyNormal;
	TArray<FVector> boxSidePoints;

	if (cma && MOI->GetAssembly().TryGetProperty(BIMPropertyNames::Normal, assemblyNormal) &&
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
void FMOIFFEImpl::InternalUpdateGeometry()
{
	ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(MOI->GetActor());
	cma->MakeFromAssembly(MOI->GetAssembly(), FVector::OneVector, MOI->GetObjectInverted(), true);

	FTransform dataStateTransform;
	const FMOIStateData &dataState = ((const FModumateObjectInstance*)MOI)->GetDataState();
	dataStateTransform.SetLocation(dataState.Location);
	dataStateTransform.SetRotation(dataState.Orientation);

	cma->SetActorTransform(dataStateTransform);
}

void FMOIFFEImpl::SetIsDynamic(bool bIsDynamic)
{
	auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
	if (meshActor)
	{
		meshActor->SetIsDynamic(bIsDynamic);
	}
}

bool FMOIFFEImpl::GetIsDynamic() const
{
	auto meshActor = Cast<ACompoundMeshActor>(MOI->GetActor());
	if (meshActor)
	{
		return meshActor->GetIsDynamic();
	}
	return false;
}