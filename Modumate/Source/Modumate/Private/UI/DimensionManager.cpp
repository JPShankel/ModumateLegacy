#include "UI/DimensionManager.h"

#include "Database/ModumateObjectEnums.h"
#include "Graph/Graph3DTypes.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UI/DimensionActor.h"
#include "UI/GraphDimensionActor.h"
#include "UI/AngleDimensionActor.h"
#include "Widgets/SWidget.h"

UDimensionManager::UDimensionManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDimensionManager::Init()
{

}

void UDimensionManager::Reset()
{
	for (auto& kvp : DimensionActors)
	{
		kvp.Value->Destroy();
	}
	DimensionActors.Empty();

	LastSelectedObjID = MOD_ID_NONE;
	LastSelectedVertexIDs.Reset();
	LastSelectedEdgeIDs.Reset();
	CurrentGraphDimensionStrings.Reset();
}

void UDimensionManager::Shutdown()
{

}

void UDimensionManager::UpdateGraphDimensionStrings(int32 selectedGraphObjID)
{
	// find which vertices are currently selected and create measuring dimension strings
	auto& doc = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	auto& graph = doc.GetVolumeGraph();

	LastSelectedVertexIDs.Reset();
	LastSelectedEdgeIDs.Reset();

	auto moi = doc.GetObjectById(selectedGraphObjID);

	// aggregate the unique selected vertices
	if (auto graphObject = graph.FindObject(selectedGraphObjID))
	{
		graphObject->GetVertexIDs(LastSelectedVertexIDs);
	}
	else if (auto parentMoi = doc.GetObjectById(moi->GetParentID()))
	{
		if (auto parentGraphObject = graph.FindObject(parentMoi->ID))
		{
			parentGraphObject->GetVertexIDs(LastSelectedVertexIDs);
			selectedGraphObjID = parentMoi->ID;
		}
	}
	// TODO: surface graph objects?

	if (LastSelectedVertexIDs.Num() == 0)
	{
		selectedGraphObjID = MOD_ID_NONE;
	}

	// previously this function checked whether the new selected object ID was different than the current one
	// however, when adjacent metaplanes are added/removed through undo/redo, there should be different
	// dimension strings even though the selection hasn't changed
	for (int32 id : CurrentGraphDimensionStrings)
	{
		ReleaseDimensionActor(id);
	}
	CurrentGraphDimensionStrings.Reset();

	// edges are editable when translating the selected object along that edge is valid
	TMap<int32, bool> edgeIDToEditability;

	// if the input selected ID was not in the graph, no new actors will be created
	graph.CheckTranslationValidity(LastSelectedVertexIDs, edgeIDToEditability);

	for (auto kvp : edgeIDToEditability)
	{
		auto dimensionActor = AddDimensionActor<AGraphDimensionActor>();
		dimensionActor->SetTarget(kvp.Key, selectedGraphObjID, kvp.Value);
		CurrentGraphDimensionStrings.Add(dimensionActor->ID);
	}

	// if a face has been selected, look at its edge's face connections 
	// and add a dimension string measuring the next and previous faces
	if (auto face = graph.FindFace(selectedGraphObjID))
	{
		LastSelectedEdgeIDs = face->EdgeIDs;
		// angle dimensions
		for (int32 edgeID : LastSelectedEdgeIDs)
		{
			auto currentEdge = graph.FindEdge(edgeID);

			auto connectedFaces = currentEdge->ConnectedFaces;
			int32 numFaces = connectedFaces.Num();

			for(int32 connectionIdx = 0; connectionIdx < numFaces; connectionIdx++)
			{
				auto& connection = connectedFaces[connectionIdx];

				if (FMath::Abs(connection.FaceID) == face->ID)
				{
					if (numFaces == 2)
					{
						// other face
						auto angleActor = AddDimensionActor<AAngleDimensionActor>();
						angleActor->SetTarget(edgeID, TPair<int32, int32>(connection.FaceID,
							connectedFaces[(connectionIdx + 1) % numFaces].FaceID));
						CurrentGraphDimensionStrings.Add(angleActor->ID);
					}
					else if (numFaces >= 3)
					{
						// previous face
						auto angleActor = AddDimensionActor<AAngleDimensionActor>();
						angleActor->SetTarget(edgeID, TPair<int32, int32>(connection.FaceID,
							connectedFaces[(connectionIdx + numFaces - 1) % numFaces].FaceID));
						CurrentGraphDimensionStrings.Add(angleActor->ID);

						// next face
						angleActor = AddDimensionActor<AAngleDimensionActor>();
						angleActor->SetTarget(edgeID, TPair<int32, int32>(connection.FaceID,
							connectedFaces[(connectionIdx + 1) % numFaces].FaceID));
						CurrentGraphDimensionStrings.Add(angleActor->ID);

					}
				}
			}
		}
	}

	LastSelectedObjID = selectedGraphObjID;
}

void UDimensionManager::ClearGraphDimensionStrings()
{
	for (int32 id : CurrentGraphDimensionStrings)
	{
		ReleaseDimensionActor(id);
	}
	CurrentGraphDimensionStrings.Reset();
	LastSelectedObjID = MOD_ID_NONE;
	LastSelectedVertexIDs.Reset();
	LastSelectedEdgeIDs.Reset();
}

ADimensionActor *UDimensionManager::GetDimensionActor(int32 id)
{ 
	return DimensionActors.Contains(id) ? DimensionActors[id] : nullptr; 
}

void UDimensionManager::ReleaseDimensionActor(int32 id)
{
	if (DimensionActors.Contains(id))
	{
		DimensionActors[id]->Destroy();
		DimensionActors.Remove(id);
	}

	if (ActiveActorID == id)
	{
		SetActiveActorID(MOD_ID_NONE);
	}
}

ADimensionActor *UDimensionManager::GetActiveActor()
{
	if (!DimensionActors.Contains(ActiveActorID))
	{
		return nullptr;
	}

	return DimensionActors[ActiveActorID];
}

void UDimensionManager::SetActiveActorID(int32 ID)
{
	ActiveActorID = ID;
}