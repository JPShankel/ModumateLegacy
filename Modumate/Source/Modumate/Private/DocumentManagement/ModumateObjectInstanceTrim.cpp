// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceTrim.h"

#include "DocumentManagement/ModumateSnappingView.h"
#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

FMOITrimImpl::FMOITrimImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, TrimStartPos(ForceInitToZero)
	, TrimEndPos(ForceInitToZero)
	, TrimNormal(ForceInitToZero)
	, TrimUp(ForceInitToZero)
	, TrimDir(ForceInitToZero)
	, TrimScale(FVector::OneVector)
	, UpperExtensions(ForceInitToZero)
	, OuterExtensions(ForceInitToZero)
{
}

FMOITrimImpl::~FMOITrimImpl()
{
}

void FMOITrimImpl::SetRotation(const FQuat &r)
{
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
}

FVector FMOITrimImpl::GetNormal() const
{
	return TrimUp;
}

FVector FMOITrimImpl::GetLocation() const
{
	const FModumateObjectInstance* parentMOI = MOI ? MOI->GetParentObject() : nullptr;
	if (parentMOI)
	{
		return parentMOI->GetObjectLocation();
	}

	return FVector::ZeroVector;
}

void FMOITrimImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* Controller)
{
	MOI->MakeHandle<AAdjustInvertHandle>();
}

void FMOITrimImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP* Controller, bool bShow)
{
	return FModumateObjectInstanceImplBase::ShowAdjustmentHandles(Controller, bShow);
}

bool FMOITrimImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		if (!UpdateCachedStructure())
		{
			return false;
		}

		MOI->MarkDirty(EObjectDirtyFlags::Mitering);
		return true;
	}
	case EObjectDirtyFlags::Mitering:
	{
		if (!UpdateMitering())
		{
			return false;
		}

		bool bInPreviewMode = MOI->GetIsInPreviewMode();
		bool bRecreateMesh = !bInPreviewMode;
		bool bCreateCollision = !bInPreviewMode;
		return InternalUpdateGeometry(bRecreateMesh, bCreateCollision);
	}
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		return true;
	default:
		return true;
	}
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

bool FMOITrimImpl::UpdateCachedStructure()
{
	const FBIMAssemblySpec& trimAssembly = MOI->GetAssembly();

	const FSimplePolygon* polyProfile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(&trimAssembly, polyProfile))
	{
		return false;
	}

	// Find the parent surface edge MOI to set up mounting.
	// This can fail gracefully, if the object is still getting set up before it has a parent assigned.
	const FModumateObjectInstance* parentMOI = MOI->GetParentObject();
	if ((parentMOI == nullptr) || (parentMOI->GetObjectType() != EObjectType::OTSurfaceEdge))
	{
		return false;
	}

	const FModumateObjectInstance* surfaceGraphMOI = parentMOI->GetParentObject();
	if ((surfaceGraphMOI == nullptr) || (surfaceGraphMOI->GetObjectType() != EObjectType::OTSurfaceGraph))
	{
		return false;
	}

	TrimStartPos = parentMOI->GetCorner(0);
	TrimEndPos = parentMOI->GetCorner(1);
	TrimDir = (TrimEndPos - TrimStartPos).GetSafeNormal();
	TrimNormal = surfaceGraphMOI->GetNormal();

	TrimUp = TrimDir ^ TrimNormal;
	if (MOI->GetObjectInverted())
	{
		TrimUp *= -1.0f;
	}

	TrimScale = FVector::OneVector;

	// TODO: store justification
	float justification = MOI->GetExtents().Z;
	float justificationDist = justification * polyProfile->Extents.GetSize().Y;
	FVector justificationDelta = justificationDist * TrimUp;

	TrimStartPos += justificationDelta;
	TrimEndPos += justificationDelta;

	return true;
}

bool FMOITrimImpl::UpdateMitering()
{
	// TODO: miter with neighboring trim, automatically based on graph connectivity and/or based on saved mitering preferences
	UpperExtensions = OuterExtensions = FVector2D::ZeroVector;
	return true;
}

bool FMOITrimImpl::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
{
	const FBIMAssemblySpec& trimAssembly = MOI->GetAssembly();

	return DynamicMeshActor->SetupExtrudedPolyGeometry(trimAssembly, TrimStartPos, TrimEndPos,
		TrimNormal, TrimUp, UpperExtensions, OuterExtensions, TrimScale, bRecreate, bCreateCollision);
}
