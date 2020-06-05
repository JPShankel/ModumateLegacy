#include "UI/DimensionManager.h"

#include "UnrealClasses/DimensionActor.h"
#include "UnrealClasses/EditModelGameState_CPP.h"

UDimensionManager::UDimensionManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDimensionManager::Init()
{

}

void UDimensionManager::Reset()
{
	for (ADimensionActor* actor : DimensionActors)
	{
		actor->Destroy();
	}
	DimensionActors.Empty();

	LastSelectedObjID = MOD_ID_NONE;
	LastSelectedVertexIDs.Reset();
}

void UDimensionManager::Shutdown()
{

}

void UDimensionManager::UpdateGraphDimensionStrings(int32 selectedGraphObjID)
{
	// find which vertices are currently selected and create measuring dimension strings
	auto& graph = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document.GetVolumeGraph();

	LastSelectedVertexIDs.Reset();

	// aggregate the unique selected vertices
	if (auto vertex = graph.FindVertex(selectedGraphObjID))
	{
		LastSelectedVertexIDs = { selectedGraphObjID };
	}
	else if (auto edge = graph.FindEdge(selectedGraphObjID))
	{
		LastSelectedVertexIDs = { edge->StartVertexID, edge->EndVertexID };
	}
	else if (auto face = graph.FindFace(selectedGraphObjID))
	{
		LastSelectedVertexIDs = face->VertexIDs;
	}
	else
	{
		selectedGraphObjID = MOD_ID_NONE;
	}

	if (selectedGraphObjID != LastSelectedObjID)
	{
		for (ADimensionActor* actor : DimensionActors)
		{
			actor->Destroy();
		}
		DimensionActors.Empty();

		// edges are editable when translating the selected object along that edge is valid
		TMap<int32, bool> edgeIDToEditability;

		// if the input selected ID was not in the graph, no new actors will be created
		graph.CheckTranslationValidity(LastSelectedVertexIDs, edgeIDToEditability);

		for (auto kvp : edgeIDToEditability)
		{
			ADimensionActor* dimensionActor = GetWorld()->SpawnActor<ADimensionActor>(ADimensionActor::StaticClass());
			dimensionActor->SetTarget(kvp.Key, selectedGraphObjID, kvp.Value);
			DimensionActors.Add(dimensionActor);
		}

		LastSelectedObjID = selectedGraphObjID;
	}
}