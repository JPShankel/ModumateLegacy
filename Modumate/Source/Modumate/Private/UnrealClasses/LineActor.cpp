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
#include "ModumateFunctionLibrary.h"
#include "Graph3D.h"

// Sets default values
ALineActor::ALineActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Tris({ 2, 1, 0, 3, 2, 0 })
	, Color(FColor::Black)
	, Thickness(1.0f)
{
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	RootComponent = ProceduralMesh;
	ProceduralMesh->bUseAsyncCooking = true;
	PrimaryActorTick.bCanEverTick = true;

	Uvs.Init(FVector2D(), 4);
	Colors.Init(Color, 4);
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

		CameraManager = playerController->PlayerCameraManager;

		SetIsHUD(bIsHUD);
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

bool ALineActor::CalculateVertices()
{
	auto *playerController = GetWorld()->GetFirstPlayerController();
	if (playerController == nullptr || CameraManager == nullptr)
	{
		return false;
	}
		
	FVector camOrigin = CameraManager->GetCameraLocation();
	FVector quadOffsetDirection = FVector::CrossProduct(Point2 - Point1, camOrigin - Point1).GetSafeNormal();
	if (quadOffsetDirection.IsZero())
	{
		return false;
	}


	FVector direction = (Point2 - Point1).GetSafeNormal();
	FVector normal = FVector::CrossProduct(direction, quadOffsetDirection).GetSafeNormal();

	if (FVector::DotProduct(normal, camOrigin) < 0)
	{
		normal *= -1.0f;
	}

	FVector offset = 0.5f * Thickness * quadOffsetDirection;

	// make a draft set of points to project on to the screen
	Vertices = {
		Point1 - offset,
		Point1 + offset,
		Point2 + offset,
		Point2 - offset
	};

	bool bOutBehindCamera;
	ScreenVertices.Reset();
	for (FVector vertex : Vertices)
	{
		FVector2D screenVertex;
		UModumateFunctionLibrary::ProjectWorldToScreenBidirectional(playerController, vertex, screenVertex, bOutBehindCamera, false);
		ScreenVertices.Add(screenVertex);
	}

	float p1Size = FMath::Clamp((ScreenVertices[1] - ScreenVertices[0]).Size(), KINDA_SMALL_NUMBER, 1000.0f);
	float p2Size = FMath::Clamp((ScreenVertices[3] - ScreenVertices[2]).Size(), KINDA_SMALL_NUMBER, 1000.0f);

	// calculate the difference between the desired screen-space scale (Thickness) and the calculated screen-space scale
	float p1Scale = Thickness / p1Size;
	float p2Scale = Thickness / p2Size;

	// update the points based on the calculated scale
	Vertices = {
		Point1 - (offset * p1Scale),
		Point1 + (offset * p1Scale),
		Point2 + (offset * p2Scale),
		Point2 - (offset * p2Scale)
	};
		
	Normals.Init(normal, 4);

	Tangents.Init(FProcMeshTangent(direction.X, direction.Y, direction.Z), 4);

	return true;
}

bool ALineActor::MakeGeometry()
{
	ProceduralMesh->ClearAllMeshSections();

	if (!CalculateVertices())
	{
		return false;
	}

	ProceduralMesh->CreateMeshSection_LinearColor(0, Vertices, Tris, Normals, Uvs, Colors, Tangents, false);
	FArchitecturalMaterial materialData;
	materialData.EngineMaterial = EMGameMode ? EMGameMode->LineMaterial : nullptr;
	if (EMGameMode != nullptr)
	{
		ProceduralMesh->SetMaterial(0, materialData.EngineMaterial.Get());
	}

	return true;
}

bool ALineActor::UpdateGeometry()
{
	if (!CalculateVertices())
	{
		return false;
	}

	ProceduralMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, Uvs, Colors, Tangents);

	return true;
}

// Called every frame
void ALineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bIsHUD)
	{
		if (bCreate)
		{
			bLastRenderValid = MakeGeometry();
			bCreate = false;
		}
		else
		{
			bLastRenderValid = UpdateGeometry();
		}

		SetActorHiddenInGame(!(bVisibleInApp && bLastRenderValid));
	}
}

void ALineActor::SetIsHUD(bool bRenderScreenSpace)
{
	if (!bRenderScreenSpace)
	{
		if (EMPlayerHUD)
		{
			EMPlayerHUD->All3DLineActors.RemoveSingleSwap(this, false);
		}
		bCreate = true;
		bIsHUD = bRenderScreenSpace;
	}
	else
	{
		EMPlayerHUD->All3DLineActors.Add(this);
		ProceduralMesh->ClearAllMeshSections();
	}
}

void ALineActor::SetVisibilityInApp(bool bVisible)
{
	bVisibleInApp = bVisible;
	SetActorHiddenInGame(!(bVisibleInApp && bLastRenderValid));
}

void ALineActor::UpdateMetaEdgeVisuals(bool bConnected, float ThicknessMultiplier)
{
	FVector normal = (Point2 - Point1).GetSafeNormal();

	// Changing the color here would require modifying the material (currently EMGameMode->LineMaterial)
	Color = FColor::Black;
	Thickness = 2.0f * ThicknessMultiplier;
}

