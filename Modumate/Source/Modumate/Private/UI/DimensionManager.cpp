#include "UI/DimensionManager.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UI/DimensionActor.h"
#include "UI/GraphDimensionActor.h"
#include "UI/AngleDimensionActor.h"

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
	auto& graph = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.GetVolumeGraph();

	LastSelectedVertexIDs.Reset();
	LastSelectedEdgeIDs.Reset();

	// aggregate the unique selected vertices
	if (auto vertex = graph.FindVertex(selectedGraphObjID))
	{
		LastSelectedVertexIDs = { selectedGraphObjID };
		LastSelectedEdgeIDs = {};
	}
	else if (auto edge = graph.FindEdge(selectedGraphObjID))
	{
		LastSelectedVertexIDs = { edge->StartVertexID, edge->EndVertexID };
		LastSelectedEdgeIDs = {};
	}
	else if (auto face = graph.FindFace(selectedGraphObjID))
	{
		LastSelectedVertexIDs = face->VertexIDs;
		LastSelectedEdgeIDs = face->EdgeIDs;
	}
	else
	{
		selectedGraphObjID = MOD_ID_NONE;
	}

	if (selectedGraphObjID != LastSelectedObjID)
	{
		for (auto& kvp : DimensionActors)
		{
			kvp.Value->Destroy();
		}
		DimensionActors.Empty();
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
}