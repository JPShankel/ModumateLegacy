// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/AngleDimensionActor.h"

#include "Components/EditableTextBox.h"
#include "Graph/Graph3D.h"
#include "UI/ArcActor.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/LineActor.h"

AAngleDimensionActor::AAngleDimensionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PendingArc(nullptr)
	, AnchorEdgeID(MOD_ID_NONE)
	, StartFaceID(MOD_ID_NONE)
	, EndFaceID(MOD_ID_NONE)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAngleDimensionActor::SetTarget(int32 EdgeID, TPair<int32, int32> FaceIDs)
{
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
	//TWeakObjectPtr<AEditModelPlayerController> playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
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

	auto& volumeGraph = Document->GetVolumeGraph();

	auto anchorEdge = volumeGraph.FindEdge(FMath::Abs(AnchorEdgeID));
	auto startFace = volumeGraph.FindFace(FMath::Abs(StartFaceID));
	auto endFace = volumeGraph.FindFace(FMath::Abs(EndFaceID));

	auto controller = DimensionText->GetOwningPlayer();

	if (!(anchorEdge && startFace && endFace && controller))
	{
		return;
	}

	auto startVertex = volumeGraph.FindVertex(anchorEdge->StartVertexID);
	auto endVertex = volumeGraph.FindVertex(anchorEdge->EndVertexID);

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
