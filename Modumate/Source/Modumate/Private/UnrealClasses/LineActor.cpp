// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "LineActor.h"
#include "ProceduralMeshComponent.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerHUD_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameState_CPP.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Graph3D.h"

// Sets default values
ALineActor::ALineActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Color(FColor::Green)
	, Thickness(1.0f)
{
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	RootComponent = ProceduralMesh;
	ProceduralMesh->bUseAsyncCooking = true;
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ALineActor::BeginPlay()
{
	Super::BeginPlay();

	if (auto *playerController = GetWorld()->GetFirstPlayerController())
	{
		EMPlayerState = Cast<AEditModelPlayerState_CPP>(playerController->PlayerState);
		EMGameMode = EMPlayerState->GetEditModelGameMode();
		EMPlayerHUD = Cast<AEditModelPlayerHUD_CPP>(playerController->GetHUD());
		EMGameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
		EMPlayerHUD->All3DLineActors.Add(this);
	}
}

void ALineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EMPlayerHUD)
	{
		EMPlayerHUD->All3DLineActors.RemoveSingleSwap(this, false);
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ALineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ALineActor::UpdateMetaEdgeVisuals(bool bConnected, float ThicknessMultiplier)
{
	FVector normal = (Point2 - Point1).GetSafeNormal();

	Color = EMPlayerState->GetMetaPlaneColor(normal, false, bConnected);
	Thickness = 2.0f * ThicknessMultiplier;
}

