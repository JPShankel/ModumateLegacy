// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/RoofPerimeterHandles.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"

namespace Modumate
{
	bool FCreateRoofFacesHandle::OnBeginUse()
	{
		UWorld *world = Handle.IsValid() ? Handle->GetWorld() : nullptr;
		AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;

		if (gameState == nullptr)
		{
			return false;
		}

		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnEndUse, but it currently creates an FMOIDelta unconditionally
		bIsInUse = true;

		if (UModumateObjectStatics::GetRoofGeometryValues(MOI->GetControlPoints(), MOI->GetControlPointIndices(), EdgePoints, EdgeSlopes, EdgesHaveFaces, EdgeIDs) &&
			UModumateGeometryStatics::TessellateSlopedEdges(EdgePoints, EdgeSlopes, EdgesHaveFaces, CombinedPolyVerts, PolyVertIndices))
		{
			int32 numEdges = EdgeIDs.Num();
			int32 numCalculatedPolys = PolyVertIndices.Num();
			if (numEdges == numCalculatedPolys)
			{
				const FVector *combinedPolyVertsPtr = CombinedPolyVerts.GetData();
				int32 vertIdxStart = 0, vertIdxEnd = 0;

				FModumateDocument &doc = gameState->Document;
				// TODO: we shouldn't need the volume graph at all for FGraph3D::GetDeltaForFaceAddition, let alone a mutable pointer to it.
				FGraph3D *volumeGraph = const_cast<FGraph3D*>(&doc.GetVolumeGraph());
				FGraph3D *tempVolumeGraph = &doc.GetTempVolumeGraph();
				int32 nextID = doc.GetNextAvailableID();

				bool bFaceAdditionFailure = false;
				TArray<FGraph3DDelta> graphDeltas;

				for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
				{
					vertIdxEnd = PolyVertIndices[edgeIdx];

					if (EdgesHaveFaces[edgeIdx])
					{
						int32 numPolyVerts = (vertIdxEnd - vertIdxStart) + 1;
						TArray<FVector> polyVerts(combinedPolyVertsPtr + vertIdxStart, numPolyVerts);

						UE_LOG(LogTemp, Log, TEXT("Add roof face #%d with %d verts"), edgeIdx + 1, numPolyVerts);
						int32 existingFaceID;
						bool bAddedFace = tempVolumeGraph->GetDeltaForFaceAddition(polyVerts, graphDeltas, nextID, existingFaceID);
						if (!bAddedFace)
						{
							bFaceAdditionFailure = true;
							break;
						}
					}

					vertIdxStart = vertIdxEnd + 1;
				}

				FGraph3D::CloneFromGraph(*tempVolumeGraph, *volumeGraph);

				if (bFaceAdditionFailure)
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to generate graph deltas for all requested roof faces :("));
				}

				if (graphDeltas.Num() > 0)
				{
					TArray<TSharedPtr<FDelta>> deltaPtrs;
					for (FGraph3DDelta &graphDelta : graphDeltas)
					{
						deltaPtrs.Add(MakeShareable(new FGraph3DDelta(graphDelta)));
					}

					bool bAppliedDeltas = doc.ApplyDeltas(deltaPtrs, world);

					if (!bAppliedDeltas)
					{
						UE_LOG(LogTemp, Warning, TEXT("Failed to apply graph deltas for all requested roof faces :("));
					}
				}
			}
		}

		return OnEndUse();
	}

	bool FCreateRoofFacesHandle::OnEndUse()
	{
		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnEndUse, but it currently creates an FMOIDelta unconditionally
		bIsInUse = false;
		return false;
	}

	FVector FCreateRoofFacesHandle::GetAttachmentPoint()
	{
		return MOI->GetObjectLocation();
	}


	bool FRetractRoofFacesHandle::OnBeginUse()
	{
		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnEndUse, but it currently sets preview state on the target MOI unconditionally
		bIsInUse = true;

		UE_LOG(LogTemp, Log, TEXT("Use roof retract faces handle on roof #%d"), MOI->ID);

		return OnEndUse();
	}

	bool FRetractRoofFacesHandle::OnEndUse()
	{
		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnEndUse, but it currently creates an FMOIDelta unconditionally
		bIsInUse = false;
		return false;
	}

	FVector FRetractRoofFacesHandle::GetAttachmentPoint()
	{
		return MOI->GetObjectLocation();
	}
}
