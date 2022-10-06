// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/FFE.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "ToolsAndAdjustments/Handles/AdjustFFERotateHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustFFEInvertHandle.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/ModumateObjectInstanceParts.h"
#include "Objects/ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Quantities/QuantitiesManager.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"

class AEditModelPlayerController;

AMOIFFE::AMOIFFE()
	: AModumateObjectInstance()
	, CachedLocation(ForceInitToZero)
	, CachedRotation(ForceInit)
	, CachedFaceNormal(ForceInitToZero)
{}

AActor *AMOIFFE::CreateActor(const FVector &loc, const FQuat &rot)
{
	return GetWorld()->SpawnActor<ACompoundMeshActor>(ACompoundMeshActor::StaticClass(), FTransform(rot, loc));
}

FVector AMOIFFE::GetLocation() const
{
	if (GetActor() != nullptr)
	{
		return GetActor()->GetActorLocation();
	}

	return CachedLocation;
}

FQuat AMOIFFE::GetRotation() const
{
	if (GetActor() != nullptr)
	{
		return GetActor()->GetActorQuat();
	}

	return CachedRotation;
}

void AMOIFFE::SetupDynamicGeometry()
{
	// TODO: re-implement wall-mounted FF&E, either with optional surface graphs meta vertex mounts
	InternalUpdateGeometry();
}

void AMOIFFE::UpdateDynamicGeometry()
{
	InternalUpdateGeometry();
}

void AMOIFFE::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(GetActor());
	FVector assemblyNormal = GetAssembly().Normal;
	TArray<FVector> boxSidePoints;

	if (cma && UModumateObjectStatics::GetFFEBoxSidePoints(cma, assemblyNormal, boxSidePoints))
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

void AMOIFFE::InternalUpdateGeometry()
{
	ACompoundMeshActor *cma = Cast<ACompoundMeshActor>(GetActor());
	cma->MakeFromAssemblyPartAsync(FAssetRequest(GetAssembly(), nullptr), 0, FVector::OneVector, InstanceData.bLateralInverted, true);

	FTransform dataStateTransform;
	dataStateTransform.SetLocation(InstanceData.Location);
	dataStateTransform.SetRotation(InstanceData.Rotation);

	cma->SetActorTransform(dataStateTransform);
}

void AMOIFFE::SetIsDynamic(bool bIsDynamic)
{
	auto meshActor = Cast<ACompoundMeshActor>(GetActor());
	if (meshActor)
	{
		meshActor->SetIsDynamic(bIsDynamic);
	}
}

bool AMOIFFE::GetIsDynamic() const
{
	auto meshActor = Cast<ACompoundMeshActor>(GetActor());
	if (meshActor)
	{
		return meshActor->GetIsDynamic();
	}
	return false;
}

bool AMOIFFE::GetInvertedState(FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIFFEData modifiedFFEData = InstanceData;
	modifiedFFEData.bLateralInverted = !modifiedFFEData.bLateralInverted;

	return OutState.CustomData.SaveStructData(modifiedFFEData);
}

bool AMOIFFE::GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIFFEData modifiedFFEData = InstanceData;
	modifiedFFEData.Location = Transform.GetLocation();
	modifiedFFEData.Rotation = Transform.GetRotation();

	return OutState.CustomData.SaveStructData(modifiedFFEData);
}

void AMOIFFE::UpdateQuantities()
{
	CachedQuantities.Empty();
	CachedQuantities.AddQuantity(CachedAssembly.UniqueKey(), 1.0f);
	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}

bool AMOIFFE::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	QuantitiesVisitor.AddQuantity(CachedAssembly.UniqueKey(), 1.0f);

	return true;
}

bool AMOIFFE::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	SetupDynamicGeometry();
	if (DirtyFlag == EObjectDirtyFlags::Visuals)
	{
		return TryUpdateVisuals();
	}
	return true;
}