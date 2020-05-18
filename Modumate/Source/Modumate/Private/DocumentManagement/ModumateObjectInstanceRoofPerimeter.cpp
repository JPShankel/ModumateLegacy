// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceRoofPerimeter.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "Graph3D.h"
#include "LineActor.h"
#include "ModumateDocument.h"
#include "ModumateGeometryStatics.h"
#include "ModumateObjectStatics.h"
#include "ModumateVertexActor_CPP.h"
#include "RoofPerimeterHandles.h"

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
		if (CreateFacesHandleActor.IsValid())
		{
			return;
		}

		UStaticMesh *anchorMesh = Controller->EMPlayerState->GetEditModelGameMode()->AnchorMesh;

		FCreateRoofFacesHandle *handleImpl = new FCreateRoofFacesHandle(MOI);
		CreateFacesHandleActor = Controller->GetWorld()->SpawnActor<AAdjustmentHandleActor_CPP>();
		CreateFacesHandleActor->SetActorMesh(anchorMesh);
		CreateFacesHandleActor->SetHandleScale(FVector(0.001f));
		CreateFacesHandleActor->SetHandleScaleScreenSize(FVector(0.001f));

		handleImpl->Handle = CreateFacesHandleActor.Get();
		CreateFacesHandleActor->Implementation = handleImpl;
		CreateFacesHandleActor->AttachToActor(PerimeterActor.Get(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	void FMOIRoofPerimeterImpl::ClearAdjustmentHandles(AEditModelPlayerController_CPP *Controller)
	{
		if (CreateFacesHandleActor.IsValid())
		{
			CreateFacesHandleActor->Destroy();
			CreateFacesHandleActor.Reset();
		}
	}

	void FMOIRoofPerimeterImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow)
	{
		SetupAdjustmentHandles(Controller);

		if (!ensure(CreateFacesHandleActor.IsValid()))
		{
			return;
		}

		CreateFacesHandleActor->SetEnabled(bShow);
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

	bool FMOIRoofPerimeterImpl::UpdatePerimeterIDs()
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

		CachedEdgeIDs.Reset();
		if (volumeGraph.Create2DGraph(TempGroupMembers, TempConnectedGraphIDs, CachedPerimeterGraph, CachedPlane, true, false))
		{
			const FGraphPolygon *perimeterPoly = CachedPerimeterGraph.GetExteriorPolygon();
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

		// TODO: don't store the ordered edge list and edge data in ControlIndices; use strongly-typed BIM values instead somehow
		int32 numEdges = CachedEdgeIDs.Num();
		TArray<int32> controlIndices;
		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			controlIndices.Add(1);
		}
		controlIndices.Append(CachedEdgeIDs);
		MOI->SetControlPointIndices(controlIndices);

		return (numEdges > 0);
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

		// TODO: don't store the cached edge positions and edge slopes in ControlPoints; use strongly-typed BIM values instead somehow
		TArray<FVector> controlPoints = CachedPerimeterPoints;
		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			controlPoints.Add(FVector(1.0f, 0.0f, 0.0f));
		}

		MOI->SetControlPoints(controlPoints);

		FVector perimeterAxisX, perimeterAxisY;
		UModumateGeometryStatics::AnalyzeCachedPositions(CachedPerimeterPoints, CachedPlane, perimeterAxisX, perimeterAxisY, TempPerimeterPoints2D, CachedPerimeterCenter, false);
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
					lineActor->Point1 = startVertex->Position;
					lineActor->Point2 = endVertex->Position;
					lineActor->Color = lineColor;
					lineActor->Thickness = lineThickness;
				}
			}
		}
	}
}
