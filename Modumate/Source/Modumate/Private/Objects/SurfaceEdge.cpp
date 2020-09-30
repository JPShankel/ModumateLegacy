// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfaceEdge.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/SurfaceGraph.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"

FMOISurfaceEdgeImpl::FMOISurfaceEdgeImpl(FModumateObjectInstance *moi)
	: FMOIEdgeImplBase(moi)
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
	auto surfaceGraphObj = MOI ? MOI->GetParentObject() : nullptr;
	auto doc = MOI ? MOI->GetDocument() : nullptr;
	if (!ensure(surfaceGraphObj && doc && LineActor.IsValid()))
	{
		return false;
	}

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		FVector offsetDelta = FMOISurfaceGraphImpl::VisualNormalOffset * surfaceGraphObj->GetNormal();

		auto surfaceGraph = doc->FindSurfaceGraph(surfaceGraphObj->ID);
		auto surfaceEdge = surfaceGraph ? surfaceGraph->FindEdge(MOI->ID) : nullptr;
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

		CachedDeprojectedStart = startVertexObj->GetObjectLocation();
		CachedDeprojectedEnd = endVertexObj->GetObjectLocation();
		LineActor->Point1 = CachedDeprojectedStart + offsetDelta;
		LineActor->Point2 = CachedDeprojectedEnd + offsetDelta;
		break;
	}
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		break;
	default:
		break;
	}

	if (MOI)
	{
		MOI->GetConnectedMOIs(CachedConnectedMOIs);
		for (FModumateObjectInstance* connectedMOI : CachedConnectedMOIs)
		{
			if (connectedMOI->GetObjectType() == EObjectType::OTSurfacePolygon)
			{
				connectedMOI->MarkDirty(DirtyFlag);
			}
		}
	}

	return true;
}

void FMOISurfaceEdgeImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	if (MOI && LineActor.IsValid())
	{
		AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(LineActor->GetWorld()->GetFirstPlayerController()->PlayerState);

		bool bHaveChildren = (MOI->GetChildIDs().Num() > 0);
		auto controller = MOI->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
		switch (controller->EMPlayerState->GetSelectedViewMode())
		{
		case EEditViewModes::SurfaceGraphs:
			bOutVisible = true;
			bOutCollisionEnabled = true;
			break;
		case EEditViewModes::ObjectEditing:
			bOutVisible = !bHaveChildren;
			bOutCollisionEnabled = !bHaveChildren;
			break;
		default:
			bOutVisible = false;
			bOutCollisionEnabled = false;
			break;
		}

		LineActor->SetVisibilityInApp(bOutVisible);
		if (bOutVisible)
		{
			float thicknessMultiplier = GetThicknessMultiplier();
			if (MOI->IsHovered() && emPlayerState && emPlayerState->ShowHoverEffects)
			{
				LineActor->Color = HoverColor;
				LineActor->Thickness = HoverThickness * thicknessMultiplier;
			}
			else
			{
				LineActor->UpdateMetaEdgeVisuals(true, thicknessMultiplier);
			}
		}

		LineActor->SetActorEnableCollision(bOutCollisionEnabled);
	}
}

void FMOISurfaceEdgeImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller)
{
	if (MOI->HasAdjustmentHandles())
	{
		return;
	}

	// Edges always have two vertices
	for (int32 i = 0; i < 2; ++i)
	{
		auto vertexHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		vertexHandle->SetTargetIndex(i);
	}
}
