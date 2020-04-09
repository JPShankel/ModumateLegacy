// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "LineActor3D_CPP.h"
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
ALineActor3D_CPP::ALineActor3D_CPP(const FObjectInitializer& ObjectInitializer)
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
void ALineActor3D_CPP::BeginPlay()
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

void ALineActor3D_CPP::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EMPlayerHUD)
	{
		EMPlayerHUD->All3DLineActors.RemoveSingleSwap(this, false);
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ALineActor3D_CPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bDrawVerticalPlane && (EMPlayerState != nullptr) && (EMGameMode != nullptr))
	{
		FShoppingItem shopItem = EMPlayerState->GetAssemblyForToolMode(EMPlayerState->EMPlayerController->GetToolMode());
		if ((shopItem.FormattedKey != OldKey) || (shopItem.FormattedKey.Len() == 0) || (OldPoint1 != Point1) || (OldPoint2 != Point2))
		{
			UpdateVerticalPlane(shopItem);
		}
	}
}

void ALineActor3D_CPP::UpdateVerticalPlane(FShoppingItem shopItem)
{
	AssemblyShoppingItems = EMGameState->GetComponentsForAssembly(EMPlayerState->EMPlayerController->GetToolMode(), shopItem);
	TArray<FShoppingItem> revAssemblyShoppingItems;
	float sumThickness = 0.f;
	for (int32 i = AssemblyShoppingItems.Num() - 1; i >= 0; i--)
	{
		revAssemblyShoppingItems.Add(AssemblyShoppingItems[i]);
		sumThickness = sumThickness + AssemblyShoppingItems[i].Thickness;
	}
	ProceduralMesh->ClearAllMeshSections();
	ProceduralMesh->SetWorldLocation((Point1 + Point2) / 2.f);
	TArray<FVector> normals;
	for (int32 i = 0; i < 4; i++)
	{
		normals.Add(UKismetMathLibrary::RotateAngleAxis((Point2 - Point1).GetSafeNormal(), 90.f, FVector(0.f, 0.f, 1.f)));
	}
	float currentHeightZ = 0.f;
	float currentBaseZ = 0.f;
	if (Inverted)
	{
		currentBaseZ = sumThickness * -1.f;
	}
	TArray<FVector> vertices;
	TArray<FVector2D> uv0;
	TArray<int32> triangles = { 3, 0, 1, 3, 1, 2 };
	TArray<FProcMeshTangent> tangents;
	TArray<FLinearColor> vertexColors;
	for (int32 i = 0; i < revAssemblyShoppingItems.Num(); i++)
	{
		currentHeightZ = currentBaseZ + revAssemblyShoppingItems[i].Thickness;
		FVector vert1 = Point1 - ((Point1 + Point2) / 2.f);
		FVector vert2 = Point2 - ((Point1 + Point2) / 2.f);
		vertices = { FVector(vert1.X, vert1.Y, currentBaseZ),
			FVector(vert2.X, vert2.Y, currentBaseZ),
			FVector(vert2.X, vert2.Y, currentHeightZ),
			FVector(vert1.X, vert1.Y, currentHeightZ) };
		uv0 = { FVector2D(0.f, 0.f),
			FVector2D((Point1 - Point2).Size(), 0.f),
			FVector2D((Point1 - Point2).Size(), revAssemblyShoppingItems[i].Thickness),
			FVector2D(0.f, revAssemblyShoppingItems[i].Thickness) };

		ProceduralMesh->CreateMeshSection_LinearColor(i, vertices, triangles, normals, uv0, vertexColors, tangents, false);
		ProceduralMesh->ContainsPhysicsTriMeshData(true);

		if (i < DynamicMaterials.Num())
		{

		}
		else
		{
			DynamicMaterials.Add(UMaterialInstanceDynamic::Create(AEditModelGameMode_CPP::LinePlaneSegmentMaterial, ProceduralMesh));
		}
		ProceduralMesh->SetMaterial(i, DynamicMaterials[i]);
		DynamicMaterials[i]->SetTextureParameterValue(FName("Texture"), revAssemblyShoppingItems[i].Icon.Get());
		currentBaseZ = currentHeightZ;
	}
	OldKey = shopItem.FormattedKey;
	OldPoint1 = Point1;
	OldPoint2 = Point2;
}

void ALineActor3D_CPP::UpdateVerticalPlaneFromToolModeShopItem()
{
	FShoppingItem shopItem = EMPlayerState->GetAssemblyForToolMode(EMPlayerState->EMPlayerController->GetToolMode());
	UpdateVerticalPlane(shopItem);
}

void ALineActor3D_CPP::UpdateMetaEdgeVisuals(bool bConnected, float ThicknessMultiplier)
{
	FVector normal = (Point2 - Point1).GetSafeNormal();

	Color = EMPlayerState->GetMetaPlaneColor(normal, false, bConnected);
	Thickness = 2.0f * ThicknessMultiplier;
}

