// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceRoofPerimeter.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "Graph3D.h"
#include "LineActor.h"
#include "ModumateDocument.h"
#include "ModumateObjectStatics.h"
#include "ModumateVertexActor_CPP.h"

namespace Modumate
{
	FMOIRoofPerimeterImpl::FMOIRoofPerimeterImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
	{
	}

	void FMOIRoofPerimeterImpl::SetLocation(const FVector &p)
	{
	}

	FVector FMOIRoofPerimeterImpl::GetLocation() const
	{
		return FVector::ZeroVector;
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

	void FMOIRoofPerimeterImpl::OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover)
	{
		FModumateObjectInstanceImplBase::OnCursorHoverActor(controller, EnableHover);
		UpdateLineActors();
	}

	void FMOIRoofPerimeterImpl::OnSelected(bool bNewSelected)
	{
		FModumateObjectInstanceImplBase::OnSelected(bNewSelected);
		UpdateLineActors();
	}

	void FMOIRoofPerimeterImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		int32 numEdges = CachedEdgeIDs.Num();
		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			auto edgeID = CachedEdgeIDs[edgeIdx];
			const FGraph3DEdge *edge = volumeGraph.FindEdge(edgeID);
			if (edge)
			{
				const FGraph3DVertex *startVertex = volumeGraph.FindVertex(edge->StartVertexID);
				const FGraph3DVertex *endVertex = volumeGraph.FindVertex(edge->EndVertexID);
				if (startVertex && endVertex)
				{
					outPoints.Add(FStructurePoint(startVertex->Position, edge->CachedDir, edgeIdx));
					outLines.Add(FStructureLine(startVertex->Position, endVertex->Position, edgeIdx, (edgeIdx + 1) % numEdges));
				}
			}
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
			if (!UpdatePerimeterIDs())
			{
				return false;
			}

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

	bool FMOIRoofPerimeterImpl::UpdatePerimeterIDs()
	{
		if (!ensure(MOI))
		{
			return false;
		}

		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		// Ask the graph what IDs belong to this roof perimeter group object
		TSet<FTypedGraphObjID> groupMembers;
		if (!volumeGraph.GetGroup(MOI->ID, groupMembers))
		{
			return false;
		}

		// Sort the group member IDs by graph type
		TSet<int32> vertexIDs, edgeIDs, faceIDs;
		for (auto &groupMember : groupMembers)
		{
			switch (groupMember.Value)
			{
			case EGraph3DObjectType::Vertex:
				vertexIDs.Add(groupMember.Key);
				break;
			case EGraph3DObjectType::Edge:
				edgeIDs.Add(groupMember.Key);
				break;
			case EGraph3DObjectType::Face:
				faceIDs.Add(groupMember.Key);
				break;
			default:
				break;
			}
		}

		CachedEdgeIDs.Reset();
		FGraph updatedPerimeterGraph;
		if (volumeGraph.Create2DGraph(vertexIDs, edgeIDs, faceIDs, updatedPerimeterGraph, true, false))
		{
			const FGraphPolygon *perimeterPoly = updatedPerimeterGraph.GetExteriorPolygon();
			if (ensure(perimeterPoly))
			{
				for (auto signedEdgeID : perimeterPoly->Edges)
				{
					const FGraph3DEdge *graphEdge = volumeGraph.FindEdge(signedEdgeID);
					if (graphEdge && graphEdge->GroupIDs.Contains(MOI->ID))
					{
						// Perimeters may double back if they're not a closed loop, but we still want to only include each edge once.
						// However, rather than just storing their unsigned IDs, we want to preserve the directions of the edges so that the traversal order is preserved.
						if (!CachedEdgeIDs.Contains(-signedEdgeID) && !CachedEdgeIDs.Contains(signedEdgeID))
						{
							CachedEdgeIDs.Add(signedEdgeID);
						}
					}
					else
					{
						return false;
					}
				}
			}
		}

		return (CachedEdgeIDs.Num() > 0);
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

		for (FSignedID edgeID : CachedEdgeIDs)
		{
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
		for (FSignedID edgeIDToRemove : TempEdgeIDsToRemove)
		{
			TWeakObjectPtr<ALineActor> lineToRemove;
			LineActors.RemoveAndCopyValue(edgeIDToRemove, lineToRemove);
			if (lineToRemove.IsValid())
			{
				lineToRemove->Destroy();
			}
		}

		// Add new line actors, just the key so we can spawn them all later
		for (FSignedID edgeIDToAdd : TempEdgeIDsToAdd)
		{
			LineActors.Add(edgeIDToAdd);
		}

		// Update existing line actors
		bool bMOIVisible = MOI->IsVisible();
		bool bMOICollisionEnabled = MOI->IsCollisionEnabled();
		bool bMOISelected = MOI->IsSelected();
		bool bMOIHovered = MOI->IsHovered();
		FColor lineColor = bMOIHovered ? FColor::White : FColor::Purple;
		float lineThickness = bMOISelected ? 6.0f : 3.0f;

		for (auto &kvp : LineActors)
		{
			FSignedID edgeID = kvp.Key;
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
					lineActor->Point1 = (edgeID > 0) ? startVertex->Position : endVertex->Position;
					lineActor->Point2 = (edgeID > 0) ? endVertex->Position : startVertex->Position;
					lineActor->Color = lineColor;
					lineActor->Thickness = lineThickness;
				}
			}
		}
	}
}
