#include "UI/GraphDimensionActor.h"

#include "Components/EditableTextBox.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/LineActor.h"

AGraphDimensionActor::AGraphDimensionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGraphDimensionActor::SetTarget(int32 InTargetEdgeID, int32 InTargetObjID, bool bIsEditable)
{
	TargetEdgeID = InTargetEdgeID;
	TargetObjID = InTargetObjID;

	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	if (!ensure(GameState))
	{
		return;
	}
	Graph = &GameState->Document.GetVolumeGraph();

	if (DimensionText != nullptr)
	{
		DimensionText->SetIsEditable(bIsEditable);
	}
}

ALineActor* AGraphDimensionActor::GetLineActor()
{
	// the relevant actor for this implementation of the dimension interface is contained within the 
	// associated graph edge

	// TODO: this could be useful if something externally should set properties of the line, following the interface
	//return Cast<ALineActor>(GameState->Document.GetObjectById(TargetEdgeID)->GetActor());

	ensureAlways(false);
	return nullptr;
}

void AGraphDimensionActor::BeginPlay()
{
	Super::BeginPlay();

	CreateWidget();
	DimensionText->Measurement->OnTextCommitted.AddDynamic(this, &AGraphDimensionActor::OnMeasurementTextCommitted);
}

void AGraphDimensionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWidget();

	Super::EndPlay(EndPlayReason);
}

void AGraphDimensionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto targetEdge = Graph->FindEdge(FMath::Abs(TargetEdgeID));

	auto controller = DimensionText->GetOwningPlayer();

	FVector2D targetScreenPosition;
	if (targetEdge && controller &&
		controller->ProjectWorldLocationToScreen(targetEdge->CachedMidpoint, targetScreenPosition))
	{
		auto startVertex = Graph->FindVertex(targetEdge->StartVertexID);
		auto endVertex = Graph->FindVertex(targetEdge->EndVertexID);

		// Calculate angle/offset
		// if the target object is a face, determine the offset direction from the cached edge normals 
		// so that the measurement doesn't overlap with the handles
		FVector2D projStart, projEnd, edgeDirection, offsetDirection;
		controller->ProjectWorldLocationToScreen(startVertex->Position, projStart);
		controller->ProjectWorldLocationToScreen(endVertex->Position, projEnd);

		edgeDirection = projEnd - projStart;
		edgeDirection.Normalize();

		auto targetFace = Graph->FindFace(TargetObjID);
		bool bOutSameDirection;
		int32 edgeIdx = targetFace ? targetFace->FindEdgeIndex(TargetEdgeID, bOutSameDirection) : INDEX_NONE;
		if (targetFace && edgeIdx != INDEX_NONE)
		{
			FVector offset = targetFace->CachedEdgeNormals[edgeIdx];
			FVector2D screenPositionOffset;
			controller->ProjectWorldLocationToScreen(targetEdge->CachedMidpoint + offset, screenPositionOffset);
			offsetDirection = (targetScreenPosition - screenPositionOffset);
			offsetDirection.Normalize();
		}
		else
		{
			// rotate by 90 degrees in 2D
			offsetDirection = FVector2D(edgeDirection.Y, -edgeDirection.X);
		}

		if (DimensionText != nullptr)
		{
			float length = (endVertex->Position - startVertex->Position).Size();
			DimensionText->UpdateTransform(targetScreenPosition, edgeDirection, offsetDirection, length);
		}
	}
}

void AGraphDimensionActor::OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	bool bVerticesMoved = false;
	if (CommitMethod == ETextCommit::OnEnter)
	{
		float lengthValue;
		// get the desired length from the text (assuming the text is entered in feet)
		// shrinking the edge be zero length is not allowed
		if (UModumateDimensionStatics::TryParseInputNumber(Text.ToString(), lengthValue) && lengthValue > Graph->Epsilon)
		{
			lengthValue *= Modumate::InchesPerFoot * Modumate::InchesToCentimeters;

			// construct deltas

			TArray<int32> vertexIDs;

			// gather vertices that will be translated (all vertices contained within the selected object)
			if (auto vertex = Graph->FindVertex(TargetObjID))
			{
				vertexIDs = { TargetObjID };
			}
			else if (auto edge = Graph->FindEdge(TargetObjID))
			{
				vertexIDs = { edge->StartVertexID, edge->EndVertexID };
			}
			else if (auto face = Graph->FindFace(TargetObjID))
			{
				vertexIDs = face->VertexIDs;
			}

			// the selected object will be translated towards the vertex that it does not contain
			auto edge = Graph->FindEdge(TargetEdgeID);
			auto startVertex = Graph->FindVertex(edge->StartVertexID);
			auto endVertex = Graph->FindVertex(edge->EndVertexID);
			FVector offsetDir = vertexIDs.Find(startVertex->ID) != INDEX_NONE ?
				edge->CachedDir : edge->CachedDir * -1.0f;

			// lengthValue is the desired resulting length of the edge after the translation,
			// so subtract from the current value to translate by the difference
			float edgeLength = (endVertex->Position - startVertex->Position).Size();
			float offsetMagnitude = edgeLength - lengthValue;
			FVector offset = offsetDir * offsetMagnitude;

			TArray<FVector> positions;
			for (int32 vertexID : vertexIDs)
			{
				auto vertex = Graph->FindVertex(vertexID);
				positions.Add(vertex->Position + offset);
			}

			auto &doc = GameState->Document;
			bVerticesMoved = doc.MoveMetaVertices(GetWorld(), vertexIDs, positions);

		}
	}

	if (bVerticesMoved)
	{
		auto edge = Graph->FindEdge(TargetEdgeID);
		auto startVertex = Graph->FindVertex(edge->StartVertexID);
		auto endVertex = Graph->FindVertex(edge->EndVertexID);
		float length = (endVertex->Position - startVertex->Position).Size();

		DimensionText->UpdateText(length);
	}
	else
	{
		DimensionText->ResetText();
	}
}
