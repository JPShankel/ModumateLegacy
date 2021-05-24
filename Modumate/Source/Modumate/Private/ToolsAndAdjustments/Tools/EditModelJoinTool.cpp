// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelJoinTool.h"

#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

#include "DocumentManagement/ModumateCommands.h"



UJoinTool::UJoinTool()
	: Super()
{

}

bool UJoinTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SetShowHoverEffects(true);

	return UEditModelToolBase::Activate();
}

bool UJoinTool::Deactivate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;
	Controller->EMPlayerState->SetShowHoverEffects(false);

	return UEditModelToolBase::Deactivate();
}

bool UJoinTool::BeginUse()
{
	return UEditModelToolBase::BeginUse();
}

bool UJoinTool::HandleMouseUp()
{
	AEditModelGameState *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;

	AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;
		
	if (newTarget && newTarget->GetObjectType() == EObjectType::OTMetaPlane)
	{
		ActiveObjectType = newTarget->GetObjectType();

		int32 newObjID = newTarget->ID;

		const FGraph3D& volumeGraph = doc->GetVolumeGraph();
		const FGraph3DFace* face = volumeGraph.FindFace(newObjID);
		if (!ensure(face != nullptr))
		{
			return false;
		}

		ActivePlane = face->CachedPlane;

		if (PendingObjectIDs.Num() == 0 || FrontierObjectIDs.Contains(newObjID))
		{
			PendingObjectIDs.Add(newObjID);

			TSet<int32> adjFaceIDs;
			face->GetAdjacentFaceIDs(adjFaceIDs);
			for (auto faceID : adjFaceIDs)
			{
				auto adjFace = volumeGraph.FindFace(faceID);
				if (adjFace != nullptr && FVector::Parallel(adjFace->CachedPlane, ActivePlane))
				{
					FrontierObjectIDs.Add(faceID);
				}
			}

			// joining a contained face with its parent results in deleting the contained face
			for (int32 faceID : face->ContainedFaceIDs)
			{
				FrontierObjectIDs.Add(faceID);
			}
			if (face->ContainingFaceID != MOD_ID_NONE)
			{
				FrontierObjectIDs.Add(face->ContainingFaceID);
			}
		}

		Controller->EMPlayerState->SetObjectSelected(newTarget, true, false);
		// TODO: OnSelected shows the adjustment handles, and we don't want them shown here.
		// preferably there would be a way to show the metaplane as selected without showing the adjustment handles
		newTarget->ShowAdjustmentHandles(Controller, false);
	}
	else if (newTarget && newTarget->GetObjectType() == EObjectType::OTMetaEdge)
	{
		ActiveObjectType = newTarget->GetObjectType();

		int32 newObjID = newTarget->ID;
		const FGraph3D& volumeGraph = doc->GetVolumeGraph();
		const FGraph3DEdge* edge = volumeGraph.FindEdge(newObjID);
		if (!ensure(edge != nullptr))
		{
			return false;
		}

		auto startVertex = volumeGraph.FindVertex(edge->StartVertexID);
		auto endVertex = volumeGraph.FindVertex(edge->EndVertexID);

		ActiveEdge = TPair<FVector, FVector>(startVertex->Position, endVertex->Position);
		FVector originalDirection = endVertex->Position - startVertex->Position;

		if (PendingObjectIDs.Num() == 0 || FrontierObjectIDs.Contains(newObjID))
		{
			PendingObjectIDs.Add(newObjID);

			TSet<int32> adjEdgeIDs;
			edge->GetAdjacentEdgeIDs(adjEdgeIDs);

			for (auto edgeID : adjEdgeIDs)
			{
				auto adjEdge = volumeGraph.FindEdge(edgeID);
				auto adjStartVertex = volumeGraph.FindVertex(adjEdge->StartVertexID);
				auto adjEndVertex = volumeGraph.FindVertex(adjEdge->EndVertexID);
				FVector currentDirection = adjEndVertex->Position - adjStartVertex->Position;
				if (adjEdge != nullptr && FVector::Parallel(originalDirection, currentDirection))
				{ 
					FrontierObjectIDs.Add(edgeID);
				}
			}
		}

		Controller->EMPlayerState->SetObjectSelected(newTarget, true, false);
	}

	return true;
}

bool UJoinTool::EnterNextStage()
{
	AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;

	// TODO: currently executes the command on the second plane press,
	// make each plane press update the Frontier set and find a different time to execute the command
	if (newTarget && FrontierObjectIDs.Contains(newTarget->ID))
	{
		PendingObjectIDs.Add(newTarget->ID);
		Controller->DeselectAll();
		
		AEditModelGameState* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
		UModumateDocument* doc = gameState->Document;
		return doc->JoinMetaObjects(GetWorld(), PendingObjectIDs.Array());
		// TODO: potentially return the result of this function, if it is true the two faces will be joined
		// and the tool can continue
	}

	return false;
}

bool UJoinTool::FrameUpdate()
{
	Super::FrameUpdate();
	// TODO: potentially only allow selectable objects to be highlighted

	return true;
}

bool UJoinTool::EndUse()
{
	Controller->DeselectAll();

	PendingObjectIDs.Reset();
	FrontierObjectIDs.Reset();
	ActivePlane = FPlane(EForceInit::ForceInitToZero);

	return UEditModelToolBase::EndUse();
}

bool UJoinTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}


