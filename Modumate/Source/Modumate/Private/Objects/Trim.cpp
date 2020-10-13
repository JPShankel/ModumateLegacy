// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Trim.h"

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

FVector FMOITrimImpl::GetLocation() const
{
	const FModumateObjectInstance* parentMOI = MOI ? MOI->GetParentObject() : nullptr;
	if (parentMOI)
	{
		return parentMOI->GetObjectLocation();
	}

	return FVector::ZeroVector;
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

FVector FMOITrimImpl::GetNormal() const
{
	return TrimUp;
}

void FMOITrimImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
}

void FMOITrimImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* Controller)
{
	MOI->MakeHandle<AAdjustInvertHandle>();
}

void FMOITrimImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP* Controller, bool bShow)
{
	return FModumateObjectInstanceImplBase::ShowAdjustmentHandles(Controller, bShow);
}

bool FMOITrimImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
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

bool FMOITrimImpl::GetInvertedState(FMOIStateData& OutState) const
{
	OutState = MOI->GetStateData();

	FMOITrimData modifiedTrimData = InstanceData;
	modifiedTrimData.bUpInverted = !modifiedTrimData.bUpInverted;

	return OutState.CustomData.SaveStructData(modifiedTrimData);
}

bool FMOITrimImpl::UpdateCachedStructure()
{
	const FBIMAssemblySpec& trimAssembly = MOI->GetAssembly();
	const FModumateDocument* doc = MOI->GetDocument();

	const FSimplePolygon* polyProfile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(&trimAssembly, polyProfile))
	{
		return false;
	}

	// Find the parent surface edge MOI to set up mounting.
	// This can fail gracefully, if the object is still getting set up before it has a parent assigned.
	const FModumateObjectInstance* surfaceEdgeMOI = MOI->GetParentObject();
	if ((surfaceEdgeMOI == nullptr) || (surfaceEdgeMOI->GetObjectType() != EObjectType::OTSurfaceEdge) || surfaceEdgeMOI->IsDirty(EObjectDirtyFlags::Structure))
	{
		return false;
	}

	const FModumateObjectInstance* surfaceGraphMOI = surfaceEdgeMOI->GetParentObject();
	if ((surfaceGraphMOI == nullptr) || (surfaceGraphMOI->GetObjectType() != EObjectType::OTSurfaceGraph) || surfaceGraphMOI->IsDirty(EObjectDirtyFlags::Structure))
	{
		return false;
	}

	auto surfaceGraph = doc->FindSurfaceGraph(surfaceGraphMOI->ID);
	auto surfaceEdge = surfaceGraph ? surfaceGraph->FindEdge(surfaceEdgeMOI->ID) : nullptr;
	if (!surfaceGraph.IsValid() || (surfaceEdge == nullptr))
	{
		return false;
	}

	// See if the trim should be offset based on its neighboring polygons
	float maxNeighboringThickness = 0.0f;
	const FModumateObjectInstance* leftPolyMOI = doc->GetObjectById(surfaceEdge->LeftPolyID);
	const FModumateObjectInstance* rightPolyMOI = doc->GetObjectById(surfaceEdge->RightPolyID);
	for (auto neighborPolyMOI : {leftPolyMOI, rightPolyMOI})
	{
		if (neighborPolyMOI && (neighborPolyMOI->GetObjectType() == EObjectType::OTSurfacePolygon))
		{
			for (auto neighborPolyChild : neighborPolyMOI->GetChildObjects())
			{
				if (neighborPolyChild->GetObjectType() == EObjectType::OTFinish)
				{
					if (neighborPolyChild->IsDirty(EObjectDirtyFlags::Structure))
					{
						return false;
					}

					float neighborFinishThickness = neighborPolyChild->CalculateThickness();
					maxNeighboringThickness = FMath::Max(maxNeighboringThickness, neighborFinishThickness);
				}
			}
		}
	}

	TrimStartPos = surfaceEdgeMOI->GetCorner(0);
	TrimEndPos = surfaceEdgeMOI->GetCorner(1);
	TrimDir = (TrimEndPos - TrimStartPos).GetSafeNormal();
	TrimNormal = surfaceGraphMOI->GetNormal();

	TrimUp = TrimDir ^ TrimNormal;
	if (InstanceData.bUpInverted)
	{
		TrimUp *= -1.0f;
	}

	TrimScale = FVector::OneVector;

	float justification = InstanceData.UpJustification;
	float justificationDist = justification * polyProfile->Extents.GetSize().Y;
	FVector justificationDelta = justificationDist * TrimUp;
	FVector neighborOffsetDelta = maxNeighboringThickness * TrimNormal;

	TrimStartPos += justificationDelta + neighborOffsetDelta;
	TrimEndPos += justificationDelta + neighborOffsetDelta;

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
