// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceRoofPerimeter.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Graph/Graph3D.h"
#include "UnrealClasses/LineActor.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "UnrealClasses/ModumateVertexActor_CPP.h"
#include "ToolsAndAdjustments/Handles/RoofPerimeterHandles.h"
#include "UI/RoofPerimeterPropertiesWidget.h"

namespace Modumate
{
	FMOIRoofPerimeterImpl::FMOIRoofPerimeterImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, bValidPerimeterLoop(false)
		, CachedPerimeterCenter(ForceInitToZero)
		, CachedPlane(ForceInitToZero)
	{
	}

	void FMOIRoofPerimeterImpl::SetLocation(const FVector &p)
	{
	}

	FVector FMOIRoofPerimeterImpl::GetLocation() const
	{
		return CachedPerimeterCenter;
	}

	FVector FMOIRoofPerimeterImpl::GetCorner(int32 index) const
	{
		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		if (CachedEdgeIDs.IsValidIndex(index))
		{
			auto edgeID = CachedEdgeIDs[index];
			const FGraph3DEdge *edge = volumeGraph.FindEdge(edgeID);
			if (edge)
			{
				const FGraph3DVertex *startVertex = volumeGraph.FindVertex(edge->StartVertexID);
				if (startVertex)
				{
					return startVertex->Position;
				}
			}
		}

		return GetLocation();
	}

	void FMOIRoofPerimeterImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
	{
		// TODO: update visibility/collision based on edit mode, whether roof faces have been created, etc.
		FModumateObjectInstanceImplBase::UpdateVisibilityAndCollision(bOutVisible, bOutCollisionEnabled);
		UpdateLineActors();
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
		for (FSignedID edgeID : CachedEdgeIDs)
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

	void FMOIRoofPerimeterImpl::OnCursorHoverActor(AEditModelPlayerController_CPP *Controller, bool bIsHovered)
	{
		FModumateObjectInstanceImplBase::OnCursorHoverActor(Controller, bIsHovered);
		UpdateLineActors();
	}

	void FMOIRoofPerimeterImpl::OnSelected(bool bNewSelected)
	{
		FModumateObjectInstanceImplBase::OnSelected(bNewSelected);
		UpdateLineActors();
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

	AActor *FMOIRoofPerimeterImpl::RestoreActor()
	{
		LineActors.Reset();
		return FModumateObjectInstanceImplBase::RestoreActor();
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

	bool FMOIRoofPerimeterImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
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
				return false;
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

		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		// Ask the graph what IDs belong to this roof perimeter group object
		if (!volumeGraph.GetGroup(MOI->ID, TempGroupMembers))
		{
			return false;
		}

		// Separate the edges from the faces, so the perimeter is only calculated out of the edges
		TempGroupEdges.Reset();
		CachedFaceIDs.Reset();
		for (const FTypedGraphObjID &groupMember : TempGroupMembers)
		{
			switch (groupMember.Value)
			{
			case EGraph3DObjectType::Edge:
				TempGroupEdges.Add(groupMember);
				break;
			case EGraph3DObjectType::Face:
				CachedFaceIDs.Add(groupMember.Key);
				break;
			default:
				break;
			}
		}

		CachedEdgeIDs.Reset();
		if (volumeGraph.Create2DGraph(TempGroupEdges, TempConnectedGraphIDs, CachedPerimeterGraph, CachedPlane, true, false))
		{
			const FGraph2DPolygon *perimeterPoly = CachedPerimeterGraph.GetExteriorPolygon();
			if (ensure(perimeterPoly))
			{
				bValidPerimeterLoop = !perimeterPoly->bHasDuplicateEdge;
				for (auto signedEdgeID : perimeterPoly->Edges)
				{
					// Make sure that each edge in the perimeter is actually associated with this perimeter's group ID,
					// otherwise the perimeter can't be created.
					const FGraph3DEdge *graphEdge = volumeGraph.FindEdge(signedEdgeID);
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
		if (!bEdgesMatchHandles || (CachedEdgeIDs != MOI->GetControlPointIndices()))
		{
			// TODO: may not need to destroy -all- of the existing handles
			MOI->ClearAdjustmentHandles();
		}

		// Update the handles regardless; this is the last opportunity to toggle visibility between face creation / retraction handles, etc.
		MOI->ShowAdjustmentHandles(playerController, bAdjustmentHandlesVisible);

		// TODO: maybe don't store the ordered edge list in ControlIndices?
		MOI->SetControlPointIndices(CachedEdgeIDs);

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
		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		int32 numEdges = CachedEdgeIDs.Num();
		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			auto edgeID = CachedEdgeIDs[edgeIdx];
			const FGraph3DEdge *edge = volumeGraph.FindEdge(edgeID);
			if (edge)
			{
				int32 vertexID = (edgeID > 0) ? edge->StartVertexID : edge->EndVertexID;
				const FGraph3DVertex *vertex = volumeGraph.FindVertex(vertexID);
				if (vertex)
				{
					CachedPerimeterPoints.Add(vertex->Position);
				}
			}
		}

		MOI->SetControlPoints(CachedPerimeterPoints);

		FVector perimeterAxisX, perimeterAxisY;
		UModumateGeometryStatics::AnalyzeCachedPositions(CachedPerimeterPoints, CachedPlane, perimeterAxisX, perimeterAxisY, TempPerimeterPoints2D, CachedPerimeterCenter, false);

		PerimeterActor->SetActorLocation(CachedPerimeterCenter);
	}

	void FMOIRoofPerimeterImpl::UpdateLineActors()
	{
		if (!ensure(MOI))
		{
			return;
		}

		// Keep track of which edges we already have actors for,
		// which edges we need to create actors for,
		// and which edges no longer need actors.
		TempEdgeIDsToAdd.Reset();
		TempEdgeIDsToRemove.Reset();

		for (auto &kvp : LineActors)
		{
			TempEdgeIDsToRemove.Add(kvp.Key);
		}

		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		for (FSignedID signedEdgeID : CachedEdgeIDs)
		{
			int32 edgeID = FMath::Abs(signedEdgeID);
			if (volumeGraph.ContainsObject(FTypedGraphObjID(edgeID, EGraph3DObjectType::Edge)))
			{
				if (!LineActors.Contains(edgeID))
				{
					TempEdgeIDsToAdd.Add(edgeID);
				}

				TempEdgeIDsToRemove.Remove(edgeID);
			}
			else
			{
				TempEdgeIDsToRemove.Remove(edgeID);
			}
		}

		// Delete outdated line actors
		for (int32 edgeIDToRemove : TempEdgeIDsToRemove)
		{
			TWeakObjectPtr<ALineActor> lineToRemove;
			LineActors.RemoveAndCopyValue(edgeIDToRemove, lineToRemove);
			if (lineToRemove.IsValid())
			{
				lineToRemove->Destroy();
			}
		}

		// Add new line actors, just the key so we can spawn them all later
		for (int32 edgeIDToAdd : TempEdgeIDsToAdd)
		{
			LineActors.Add(edgeIDToAdd);
		}

		// Update existing line actors
		bool bMOIVisible = MOI->IsVisible();
		bool bMOICollisionEnabled = MOI->IsCollisionEnabled();
		bool bMOISelected = MOI->IsSelected();
		bool bMOIHovered = MOI->IsHovered();
		FColor lineColor = bMOIHovered ? FColor::White : (bValidPerimeterLoop ? FColor::Purple : FColor::Red);
		float lineThickness = bMOISelected ? 6.0f : 3.0f;

		for (auto &kvp : LineActors)
		{
			int32 edgeID = kvp.Key;
			TWeakObjectPtr<ALineActor> &lineActor = kvp.Value;
			const FGraph3DEdge *graphEdge = volumeGraph.FindEdge(edgeID);
			if (graphEdge)
			{
				const FGraph3DVertex *startVertex = volumeGraph.FindVertex(graphEdge->StartVertexID);
				const FGraph3DVertex *endVertex = volumeGraph.FindVertex(graphEdge->EndVertexID);
				if (startVertex && endVertex)
				{
					// Spawn the actor now, expected if it's new, or if it became stale
					if (!lineActor.IsValid())
					{
						lineActor = World->SpawnActor<ALineActor>();
						lineActor->AttachToActor(PerimeterActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
					}

					// Keep the line actor with the same direction as the original graph edge, for consistency
					lineActor->SetActorHiddenInGame(!bMOIVisible);
					lineActor->SetActorEnableCollision(bMOICollisionEnabled);
					lineActor->SetIsHUD(false);
					lineActor->Point1 = startVertex->Position;
					lineActor->Point2 = endVertex->Position;
					lineActor->Color = lineColor;
					lineActor->Thickness = lineThickness;
				}
			}
		}
	}
}
