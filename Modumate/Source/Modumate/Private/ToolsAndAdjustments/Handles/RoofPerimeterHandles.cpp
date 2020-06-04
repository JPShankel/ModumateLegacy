// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/RoofPerimeterHandles.h"

#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateRoofStatics.h"
#include "UI/RoofPerimeterPropertiesWidget.h"

namespace Modumate
{
	bool FCreateRoofFacesHandle::OnBeginUse()
	{
		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnBeginUse, but it currently creates an FMOIDelta unconditionally
		UWorld *world = Handle.IsValid() ? Handle->GetWorld() : nullptr;
		Controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
		bIsInUse = true;

		if ((MOI == nullptr) || (gameState == nullptr) || !Controller.IsValid())
		{
			return OnEndUse();
		}

		if (UModumateRoofStatics::GetAllProperties(MOI, EdgePoints, EdgeIDs, DefaultEdgeProperties, EdgeProperties) &&
			UModumateRoofStatics::TessellateSlopedEdges(EdgePoints, EdgeProperties, CombinedPolyVerts, PolyVertIndices))
		{
			int32 numEdges = EdgeIDs.Num();
			int32 numCalculatedPolys = PolyVertIndices.Num();
			if (numEdges == numCalculatedPolys)
			{
				const FVector *combinedPolyVertsPtr = CombinedPolyVerts.GetData();
				int32 vertIdxStart = 0, vertIdxEnd = 0;

				FModumateDocument &doc = gameState->Document;
				const FGraph3D &volumeGraph = doc.GetVolumeGraph();
				FGraph3D &tempVolumeGraph = doc.GetTempVolumeGraph();
				int32 nextID = doc.GetNextAvailableID();
				TSet<int32> groupIDs({ MOI->ID });

				bool bFaceAdditionFailure = false;
				TArray<FGraph3DDelta> graphDeltas;

				for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
				{
					vertIdxEnd = PolyVertIndices[edgeIdx];

					if (EdgeProperties[edgeIdx].bHasFace)
					{
						int32 numPolyVerts = (vertIdxEnd - vertIdxStart) + 1;
						TArray<FVector> polyVerts(combinedPolyVertsPtr + vertIdxStart, numPolyVerts);

						int32 existingFaceID;
						bool bAddedFace = tempVolumeGraph.GetDeltaForFaceAddition(polyVerts, graphDeltas, nextID, existingFaceID, groupIDs);
						if (!bAddedFace)
						{
							bFaceAdditionFailure = true;
							break;
						}
					}

					vertIdxStart = vertIdxEnd + 1;
				}

				if (graphDeltas.Num() == 0)
				{
					bFaceAdditionFailure = true;
				}

				FGraph3D::CloneFromGraph(tempVolumeGraph, volumeGraph);

				if (bFaceAdditionFailure)
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to generate graph deltas for all requested roof faces :("));
					return OnEndUse();
				}

				TArray<TSharedPtr<FDelta>> deltaPtrs;
				for (const FGraph3DDelta &graphDelta : graphDeltas)
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
		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnBeginUse, but it currently creates an FMOIDelta unconditionally
		UWorld *world = Handle.IsValid() ? Handle->GetWorld() : nullptr;
		Controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
		bIsInUse = true;

		if ((MOI == nullptr) || (gameState == nullptr) || !Controller.IsValid())
		{
			return OnEndUse();
		}

		FModumateDocument &doc = gameState->Document;
		const FGraph3D &volumeGraph = doc.GetVolumeGraph();
		if (!volumeGraph.GetGroup(MOI->ID, TempGroupMembers))
		{
			return OnEndUse();
		}

		TempFaceIDs.Reset();
		for (const auto &groupMember : TempGroupMembers)
		{
			if (groupMember.Value == EGraph3DObjectType::Face)
			{
				TempFaceIDs.Add(groupMember.Key);
			}
		}

		// TODO: migrate this to one or more deltas, when we can delete objects that way.
		doc.DeleteObjects(TempFaceIDs, true, true);

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


	bool FEditRoofEdgeHandle::OnBeginUse()
	{
		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnBeginUse, but it currently sets preview state on the target MOI unconditionally
		UWorld *world = Handle.IsValid() ? Handle->GetWorld() : nullptr;
		Controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		AEditModelPlayerHUD *playerHUD = Controller.IsValid() ? Cast<AEditModelPlayerHUD>(Controller->GetHUD()) : nullptr;
		if (playerHUD == nullptr)
		{
			return false;
		}

		bIsInUse = true;

		PropertiesWidget = Controller->HUDDrawWidget->UserWidgetPool.GetOrCreateInstance<URoofPerimeterPropertiesWidget>(playerHUD->RoofPerimeterPropertiesClass);
		PropertiesWidget->SetTarget(MOI->ID, TargetEdgeID, Handle.Get());
		PropertiesWidget->AddToViewport();
		PropertiesWidget->SetPositionInViewport(FVector2D(0.0f, 0.0f));

		return true;
	}

	bool FEditRoofEdgeHandle::OnUpdateUse()
	{
		return PropertiesWidget.IsValid();
	}

	void FEditRoofEdgeHandle::Tick(float DeltaTime)
	{
		FEditModelAdjustmentHandleBase::Tick(DeltaTime);

		if (PropertiesWidget.IsValid())
		{
			PropertiesWidget->UpdateTransform();
		}
	}

	bool FEditRoofEdgeHandle::OnEndUse()
	{
		// TODO: we should be able to rely on FEditModelAdjustmentHandleBase's OnEndUse, but it currently creates an FMOIDelta unconditionally
		return OnAbortUse();
	}

	bool FEditRoofEdgeHandle::OnAbortUse()
	{
		bIsInUse = false;

		if (PropertiesWidget.IsValid())
		{
			PropertiesWidget->RemoveFromViewport();
			PropertiesWidget->ConditionalBeginDestroy();
		}

		return false;
	}

	FVector FEditRoofEdgeHandle::GetAttachmentPoint()
	{
		const FModumateDocument *doc = MOI ? MOI->GetDocument() : nullptr;
		const FModumateObjectInstance *targetEdgeMOI = doc ? doc->GetObjectById(FMath::Abs(TargetEdgeID)) : nullptr;
		if (ensure(targetEdgeMOI))
		{
			return targetEdgeMOI->GetObjectLocation();
		}

		return FVector::ZeroVector;
	}
}
