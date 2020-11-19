// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/RoofPerimeter.h"

#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "ToolsAndAdjustments/Handles/RoofPerimeterHandles.h"
#include "UI/RoofPerimeterPropertiesWidget.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"


FMOIRoofPerimeterImpl::FMOIRoofPerimeterImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, bValidPerimeterLoop(false)
	, CachedPerimeterCenter(ForceInitToZero)
	, CachedPlane(ForceInitToZero)
{
}

FVector FMOIRoofPerimeterImpl::GetLocation() const
{
	return CachedPerimeterCenter;
}

FVector FMOIRoofPerimeterImpl::GetCorner(int32 index) const
{
	const Modumate::FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

	if (CachedEdgeIDs.IsValidIndex(index))
	{
		auto edgeID = CachedEdgeIDs[index];
		const Modumate::FGraph3DEdge *edge = volumeGraph.FindEdge(edgeID);
		if (edge)
		{
			const Modumate::FGraph3DVertex *startVertex = volumeGraph.FindVertex(edge->StartVertexID);
			if (startVertex)
			{
				return startVertex->Position;
			}
		}
	}

	return GetLocation();
}

void FMOIRoofPerimeterImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
}

void FMOIRoofPerimeterImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	// RoofPerimeters are only seen and interacted with via their edges.
	bOutVisible = bOutCollisionEnabled = false;
}

void FMOIRoofPerimeterImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller)
{
	if (CreateFacesHandle.IsValid())
	{
		return;
	}

	UStaticMesh *anchorMesh = Controller->EMPlayerState->GetEditModelGameMode()->AnchorMesh;

	CreateFacesHandle = MOI->MakeHandle<ACreateRoofFacesHandle>();
	RetractFacesHandle = MOI->MakeHandle<ARetractRoofFacesHandle>();

	// Handles for modifying roof edges
	for (FGraphSignedID edgeID : CachedEdgeIDs)
	{
		AEditRoofEdgeHandle *editEdgeHandle = MOI->MakeHandle<AEditRoofEdgeHandle>();
		editEdgeHandle->SetTargetEdge(edgeID);

		EdgeHandlesByID.Add(edgeID, editEdgeHandle);
	}
}

void FMOIRoofPerimeterImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow)
{
	bAdjustmentHandlesVisible = bShow;

	bool bCreatedRoofFaces = (CachedFaceIDs.Num() > 0);

	if (CreateFacesHandle.IsValid())
	{
		CreateFacesHandle->SetEnabled(bShow && !bCreatedRoofFaces);
	}
		
	if (RetractFacesHandle.IsValid())
	{
		RetractFacesHandle->SetEnabled(bShow && bCreatedRoofFaces);
	}

	for (auto &kvp : EdgeHandlesByID)
	{
		TWeakObjectPtr<AEditRoofEdgeHandle> &editEdgeHandle = kvp.Value;
		if (editEdgeHandle.IsValid())
		{
			editEdgeHandle->SetEnabled(bShow && !bCreatedRoofFaces);
		}
	}
}

void FMOIRoofPerimeterImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	int32 numPoints = CachedPerimeterPoints.Num();
	for (int32 pointAIdx = 0; pointAIdx < numPoints; ++pointAIdx)
	{
		int32 pointBIdx = (pointAIdx + 1) % numPoints;

		const FVector &pointA = CachedPerimeterPoints[pointAIdx];
		const FVector &pointB = CachedPerimeterPoints[pointBIdx];
		FVector dir = (pointB - pointA).GetSafeNormal();

		outPoints.Add(FStructurePoint(pointA, dir, pointAIdx));
		outLines.Add(FStructureLine(pointA, pointB, pointAIdx, pointBIdx));
	}
}

AActor *FMOIRoofPerimeterImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	World = world;
	PerimeterActor = World->SpawnActor<AActor>();
	PerimeterActor->SetRootComponent(NewObject<USceneComponent>(PerimeterActor.Get(), USceneComponent::GetDefaultSceneRootVariableName()));

	return PerimeterActor.Get();
}

FVector FMOIRoofPerimeterImpl::GetNormal() const
{
	return CachedPlane;
}

bool FMOIRoofPerimeterImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (MOI == nullptr)
	{
		return false;
	}

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		// TODO: we shouldn't have to update the perimeter IDs every time we change geometry,
		// but we don't have a better time to do it besides cleaning structural flags.
		if (!UpdateConnectedIDs())
		{
			// If we're unable to update the graph for this RoofPerimeter, then it's not yet recoverable and must destroy itself.
			// TODO: show error state, and add the ability to manually/automatically recover a roof perimeter with the data for its remaining edges intact
			if (OutSideEffectDeltas)
			{
				// Delete the roof perimeter MOI
				auto deletionDelta = MakeShared<FMOIDelta>();
				deletionDelta->AddCreateDestroyState(MOI->GetStateData(), EMOIDeltaType::Destroy);
				OutSideEffectDeltas->Add(deletionDelta);

				// Delete the roof group from the graph
				if (TempGroupMembers.Num() > 0)
				{
					auto graphDelta = MakeShared<FGraph3DDelta>();
					for (int32 staleGroupMemberID : TempGroupMembers)
					{
						FGraph3DGroupIDsDelta groupIDsDelta(TSet<int32>(), TSet<int32>({ MOI->ID }));
						graphDelta->GroupIDsUpdates.Add(staleGroupMemberID, groupIDsDelta);
					}

					OutSideEffectDeltas->Add(graphDelta);
				}

				return true;
			}

			return false;
		}

		// Update edge handles with the latest instance data, in case they are active and displaying data for editing.
		for (auto& kvp : EdgeHandlesByID)
		{
			if (kvp.Value.IsValid())
			{
				kvp.Value->UpdateWidgetData();
			}
		}

		UpdatePerimeterGeometry();
		MOI->MarkDirty(EObjectDirtyFlags::Visuals);
		break;
	}
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		break;
	default:
		break;
	}

	return true;
}

bool FMOIRoofPerimeterImpl::UpdateConnectedIDs()
{
	if (!ensure(MOI))
	{
		return false;
	}

	const Modumate::FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

	// Ask the graph what IDs belong to this roof perimeter group object
	if (!volumeGraph.GetGroup(MOI->ID, TempGroupMembers))
	{
		return false;
	}

	// Separate the edges from the faces, so the perimeter is only calculated out of the edges
	TempGroupEdges.Reset();
	CachedFaceIDs.Reset();
	for (int32 groupMember : TempGroupMembers)
	{
		switch (volumeGraph.GetObjectType(groupMember))
		{
		case Modumate::EGraph3DObjectType::Edge:
			TempGroupEdges.Add(groupMember);
			break;
		case Modumate::EGraph3DObjectType::Face:
			CachedFaceIDs.Add(groupMember);
			break;
		default:
			break;
		}
	}

	if (!CachedPerimeterGraph.IsValid())
	{
		CachedPerimeterGraph = MakeShared<Modumate::FGraph2D>();
	}
	CachedEdgeIDs.Reset();

	if (volumeGraph.Create2DGraph(TempGroupEdges, TempConnectedGraphIDs, CachedPerimeterGraph, CachedPlane, true, false))
	{
		const Modumate::FGraph2DPolygon *perimeterPoly = CachedPerimeterGraph->GetRootPolygon();
		if (ensure(perimeterPoly))
		{
			bValidPerimeterLoop = !perimeterPoly->bHasDuplicateVertex;
			for (auto signedEdgeID : perimeterPoly->Edges)
			{
				// Make sure that each edge in the perimeter is actually associated with this perimeter's group ID,
				// otherwise the perimeter can't be created.
				const Modumate::FGraph3DEdge *graphEdge = volumeGraph.FindEdge(signedEdgeID);
				if (graphEdge && graphEdge->GroupIDs.Contains(MOI->ID))
				{
					CachedEdgeIDs.Add(signedEdgeID);
				}
				else
				{
					return false;
				}
			}
		}
	}

	// Now that we've updated the connected edge IDs for this perimeter, if either the object's previous cached edges
	// (saved as control indices) or the handles that were last created (saved in EdgeHandleActors) are out of date,
	// then we know we need to recreate the correct handles.
	bool bEdgesMatchHandles = true;
	TSet<int32> cachedEdgeIDSet(CachedEdgeIDs);
	for (auto &kvp : EdgeHandlesByID)
	{
		if (!cachedEdgeIDSet.Contains(kvp.Key))
		{
			bEdgesMatchHandles = false;
			break;
		}
	}
	for (int32 cachedEdgeID : CachedEdgeIDs)
	{
		if (!EdgeHandlesByID.Contains(cachedEdgeID))
		{
			bEdgesMatchHandles = false;
			break;
		}
	}

	// If edges are out of date, then clear out the handles
	auto playerController = Cast<AEditModelPlayerController_CPP>(World->GetFirstPlayerController());
	if (!bEdgesMatchHandles || (PrevCachedEdgeIDs != CachedEdgeIDs))
	{
		// TODO: may not need to destroy -all- of the existing handles
		MOI->ClearAdjustmentHandles();
	}

	// Update the handles regardless; this is the last opportunity to toggle visibility between face creation / retraction handles, etc.
	MOI->ShowAdjustmentHandles(playerController, bAdjustmentHandlesVisible);

	PrevCachedEdgeIDs = CachedEdgeIDs;

	return (CachedEdgeIDs.Num() > 0);
}

void FMOIRoofPerimeterImpl::UpdatePerimeterGeometry()
{
	if (!ensure(MOI) || CachedPlane.IsZero())
	{
		return;
	}

	CachedPerimeterPoints.Reset();
	CachedPerimeterCenter = FVector::ZeroVector;
	const Modumate::FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

	int32 numEdges = CachedEdgeIDs.Num();
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		auto edgeID = CachedEdgeIDs[edgeIdx];
		const Modumate::FGraph3DEdge *edge = volumeGraph.FindEdge(edgeID);
		if (edge)
		{
			int32 vertexID = (edgeID > 0) ? edge->StartVertexID : edge->EndVertexID;
			const Modumate::FGraph3DVertex *vertex = volumeGraph.FindVertex(vertexID);
			if (vertex)
			{
				CachedPerimeterPoints.Add(vertex->Position);
			}
		}
	}

	FVector perimeterAxisX, perimeterAxisY;
	UModumateGeometryStatics::AnalyzeCachedPositions(CachedPerimeterPoints, CachedPlane, perimeterAxisX, perimeterAxisY, TempPerimeterPoints2D, CachedPerimeterCenter, false);

	PerimeterActor->SetActorLocation(CachedPerimeterCenter);
}
