// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceRoofPerimeter.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "Graph3D.h"
#include "LineActor3D_CPP.h"
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

		const auto &perimeterEdgeIDs = MOI->GetControlPointIndices();
		int32 numEdges = perimeterEdgeIDs.Num();

		if (perimeterEdgeIDs.IsValidIndex(index))
		{
			auto edgeID = perimeterEdgeIDs[index];
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
		FModumateObjectInstanceImplBase::UpdateVisibilityAndCollision(bOutVisible, bOutCollisionEnabled);

		// TODO: update based on edit mode, whether roof faces have been created, etc.
	}

	void FMOIRoofPerimeterImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		const auto &perimeterEdgeIDs = MOI->GetControlPointIndices();
		int32 numEdges = perimeterEdgeIDs.Num();

		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			auto edgeID = perimeterEdgeIDs[edgeIdx];
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

	AActor *FMOIRoofPerimeterImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;
		PerimeterActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);

		return PerimeterActor.Get();
	}

	bool FMOIRoofPerimeterImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		return FModumateObjectInstanceImplBase::CleanObject(DirtyFlag);
	}

	void FMOIRoofPerimeterImpl::SetupDynamicGeometry()
	{
		UpdateLineActors();
	}

	void FMOIRoofPerimeterImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIRoofPerimeterImpl::OnSelected(bool bNewSelected)
	{
		FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

		MOI->UpdateVisibilityAndCollision();
	}

	void FMOIRoofPerimeterImpl::UpdateLineActors()
	{
		// Keep track of which edges we already have actors for,
		// which edges we need to create actors for,
		// and which edges no longer need actors.
		TSet<int32> previousEdgeIDs, edgeIDsToRemove;
		for (auto &kvp : LineActors)
		{
			previousEdgeIDs.Add(kvp.Key);
		}
		edgeIDsToRemove.Append(previousEdgeIDs);
		TArray<const FGraph3DEdge*> edgesToAdd;

		const FGraph3D &volumeGraph = MOI->GetDocument()->GetVolumeGraph();

		const TArray<int32> &perimeterEdgeIDs = MOI->GetControlPointIndices();
		for (int32 edgeID : perimeterEdgeIDs)
		{
			const FGraph3DEdge *edge = volumeGraph.FindEdge(edgeID);
			if (edge)
			{
				if (!previousEdgeIDs.Contains(edgeID))
				{
					edgesToAdd.Add(edge);
				}

				edgeIDsToRemove.Remove(edgeID);
			}
		}

		// Delete outdated line actors
		for (int32 edgeIDToRemove : edgeIDsToRemove)
		{
			TWeakObjectPtr<ALineActor3D_CPP> lineToRemove;
			if (LineActors.RemoveAndCopyValue(edgeIDToRemove, lineToRemove) && lineToRemove.IsValid())
			{
				lineToRemove->Destroy();
			}
		}

		// Add new line actors
		for (const FGraph3DEdge *edgeToAdd : edgesToAdd)
		{
			const FGraph3DVertex *startVertex = volumeGraph.FindVertex(edgeToAdd->StartVertexID);
			const FGraph3DVertex *endVertex = volumeGraph.FindVertex(edgeToAdd->EndVertexID);
			if (startVertex && endVertex)
			{
				ALineActor3D_CPP *newLine = World->SpawnActor<ALineActor3D_CPP>(ALineActor3D_CPP::StaticClass());
				newLine->Point1 = startVertex->Position;
				newLine->Point2 = endVertex->Position;
				newLine->Color = FColor::Purple;
				newLine->Thickness = 4.0f;

				TWeakObjectPtr<ALineActor3D_CPP> newLinePtr(newLine);
				LineActors.Add(edgeToAdd->ID, newLinePtr);
			}
		}
	}
}
