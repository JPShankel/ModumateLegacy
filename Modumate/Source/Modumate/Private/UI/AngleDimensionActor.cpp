#include "UI/AngleDimensionActor.h"

#include "Components/EditableTextBox.h"
#include "Graph/Graph3D.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UI/ArcActor.h"
#include "UI/EditModelPlayerHUD.h"

AAngleDimensionActor::AAngleDimensionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PendingArc(nullptr)
	, AnchorEdgeID(MOD_ID_NONE)
	, StartFaceID(MOD_ID_NONE)
	, EndFaceID(MOD_ID_NONE)
	, Graph(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAngleDimensionActor::SetTarget(int32 EdgeID, TPair<int32, int32> FaceIDs)
{
	UWorld *world = GetWorld();
	auto gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	if (!ensure(gameState))
	{
		return;
	}
	Graph = &gameState->Document.GetVolumeGraph();

	AnchorEdgeID = EdgeID;
	StartFaceID = FaceIDs.Key;
	EndFaceID = FaceIDs.Value;
}

void AAngleDimensionActor::BeginPlay()
{
	Super::BeginPlay();

	// Angle dimensions strings are read-only for the foreseeable future
	DimensionText->SetIsEditable(false);

	// TODO: make the blueprint material useable and available
	//TWeakObjectPtr<AEditModelPlayerController_CPP> playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	//AEditModelPlayerHUD *playerHUD = playerController.IsValid() ? Cast<AEditModelPlayerHUD>(playerController->GetHUD()) : nullptr;
	//PendingArc = GetWorld()->SpawnActor<AArcActor>(playerHUD->ArcClass);

	PendingArc = GetWorld()->SpawnActor<AArcActor>();
}

void AAngleDimensionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PendingArc != nullptr)
	{
		PendingArc->Destroy();
	}

	Super::EndPlay(EndPlayReason);
}

void AAngleDimensionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Graph == nullptr)
	{
		return;
	}

	auto anchorEdge = Graph->FindEdge(FMath::Abs(AnchorEdgeID));
	auto startFace = Graph->FindFace(FMath::Abs(StartFaceID));
	auto endFace = Graph->FindFace(FMath::Abs(EndFaceID));

	auto controller = DimensionText->GetOwningPlayer();

	if (!(anchorEdge && startFace && endFace && controller))
	{
		return;
	}

	auto startVertex = Graph->FindVertex(anchorEdge->StartVertexID);
	auto endVertex = Graph->FindVertex(anchorEdge->EndVertexID);

	// choose one of the vertices of the edge as the position to display the dimension widget
	FVector position = endVertex->Position.Z < startVertex->Position.Z ? endVertex->Position : startVertex->Position;

	FVector2D targetScreenPosition;
	if (!controller->ProjectWorldLocationToScreen(position, targetScreenPosition))
	{
		return;
	}

	bool bOutSameDirection;
	TArray<FVector> directions;
	directions.Add(startFace->CachedEdgeNormals[startFace->FindEdgeIndex(FMath::Abs(AnchorEdgeID), bOutSameDirection)]);
	directions.Add(endFace->CachedEdgeNormals[endFace->FindEdgeIndex(FMath::Abs(AnchorEdgeID), bOutSameDirection)]);

	if (DimensionText != nullptr)
	{
		// do not show angular dimension strings for square angles (0, 90, 180, 270)
		if (FVector::Parallel(directions[0], directions[1]) || FVector::Orthogonal(directions[0], directions[1]))
		{
			DimensionText->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			DimensionText->SetVisibility(ESlateVisibility::Visible);

			FVector offset = (directions[0] + directions[1]) / 2.0f;

			float angle = FMath::Acos(FVector::DotProduct(directions[0], directions[1]));

			FVector2D projStart, projEnd, offsetDirection;
			controller->ProjectWorldLocationToScreen(position, projStart);
			controller->ProjectWorldLocationToScreen(position + offset, projEnd);
			offsetDirection = projStart - projEnd;
			offsetDirection.Normalize();

			DimensionText->UpdateDegreeTransform(targetScreenPosition, offsetDirection, angle);
		}
	}
}
