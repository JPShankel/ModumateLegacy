// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfaceEdge.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/SurfaceGraph.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"

FMOISurfaceEdgeImpl::FMOISurfaceEdgeImpl()
	: FMOIEdgeImplBase()
	, CachedDeprojectedStart(ForceInitToZero)
	, CachedDeprojectedEnd(ForceInitToZero)
{
}

FVector FMOISurfaceEdgeImpl::GetCorner(int32 index) const
{
	return (index == 0) ? CachedDeprojectedStart : CachedDeprojectedEnd;
}

bool FMOISurfaceEdgeImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	auto surfaceGraphObj = GetParentObject();
	auto doc = GetDocument();
	if (!ensure(surfaceGraphObj && doc && LineActor.IsValid()))
	{
		return false;
	}

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		GetConnectedMOIs(CachedConnectedMOIs);

		FVector offsetDelta = FMOISurfaceGraphImpl::VisualNormalOffset * surfaceGraphObj->GetNormal();

		auto surfaceGraph = doc->FindSurfaceGraph(surfaceGraphObj->ID);
		auto surfaceEdge = surfaceGraph ? surfaceGraph->FindEdge(ID) : nullptr;
		if (!ensureAlways(surfaceEdge))
		{
			return false;
		}

		auto startVertexObj = doc->GetObjectById(surfaceEdge->StartVertexID);
		auto endVertexObj = doc->GetObjectById(surfaceEdge->EndVertexID);
		if (!ensureAlways(startVertexObj && endVertexObj) ||
			startVertexObj->IsDirty(DirtyFlag) || endVertexObj->IsDirty(DirtyFlag))
		{
			return false;
		}

		CachedDeprojectedStart = startVertexObj->GetLocation();
		CachedDeprojectedEnd = endVertexObj->GetLocation();
		LineActor->Point1 = CachedDeprojectedStart + offsetDelta;
		LineActor->Point2 = CachedDeprojectedEnd + offsetDelta;
		LineActor->UpdateTransform();
		break;
	}
	case EObjectDirtyFlags::Visuals:
		UpdateVisuals();
		break;
	default:
		break;
	}

	return true;
}
