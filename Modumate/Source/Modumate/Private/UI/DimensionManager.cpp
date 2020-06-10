#include "UI/DimensionManager.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UI/DimensionActor.h"
#include "UI/GraphDimensionActor.h"

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
			auto dimensionActor = Cast<AGraphDimensionActor>(AddDimensionActor(AGraphDimensionActor::StaticClass()));
			dimensionActor->SetTarget(kvp.Key, selectedGraphObjID, kvp.Value);
			CurrentGraphDimensionStrings.Add(dimensionActor->ID);
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