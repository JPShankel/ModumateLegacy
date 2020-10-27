// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/VertexActor.h"

#include "Engine/StaticMesh.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

// Sets default values
AVertexActor::AVertexActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// Postupdate tick group to reduce jitter when updating position
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	
	SetMobility(EComponentMobility::Movable);
}

// Called when the game starts or when spawned
void AVertexActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetStaticMeshComponent() != nullptr)
	{
		MeshComp = GetStaticMeshComponent();
		// Set the collision object type of our billboard mesh to be of type handle,
		// since it exists in screen space just like adjustment handles.
		MeshComp->SetCollisionObjectType(COLLISION_HANDLE);

		// Allow outline to be draw over the handle mesh
		MeshComp->SetRenderCustomDepth(true);
		MeshComp->SetCustomDepthStencilValue(1);
	}

	Controller = Cast<AEditModelPlayerController_CPP>(this->GetWorld()->GetFirstPlayerController());

	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

// Called every frame
void AVertexActor::Tick(float DeltaTime)
{
	FVector newCameraLoc = Controller->PlayerCameraManager->GetCameraLocation();
	if (!CameraLocation.Equals(newCameraLoc))
	{
		CameraLocation = newCameraLoc;

		UpdateVisuals();
	}
}

void AVertexActor::SetActorMesh(UStaticMesh *mesh)
{
	if (GetStaticMeshComponent() != nullptr)
	{
		MeshComp = GetStaticMeshComponent();
		MeshComp->SetStaticMesh(mesh);
	}
}

void AVertexActor::SetActorMaterial(FArchitecturalMaterial& MaterialData)
{
	Material = MaterialData;
	UModumateFunctionLibrary::SetMeshMaterial(MeshComp, Material, 0);
}

void AVertexActor::SetHandleScaleScreenSize(float NewSize)
{
	HandleScreenSize = NewSize;
	UpdateVisuals();
}

void AVertexActor::SetMOILocation(const FVector &NewMOILocation)
{
	if (!MoiLocation.Equals(NewMOILocation, 0.0f))
	{
		MoiLocation = NewMOILocation;
		UpdateVisuals();
	}
}

void AVertexActor::UpdateVisuals()
{
	FHitResult hitResult;
	FVector dirToCamera = (CameraLocation - MoiLocation).GetSafeNormal();
	static float occlusionEpsilon = 0.1f;
	FVector occlusionTraceStart = MoiLocation + (occlusionEpsilon * dirToCamera);
	bool bVertexOccluded = Controller->LineTraceSingleAgainstMOIs(hitResult, occlusionTraceStart, CameraLocation);
	MeshComp->SetVisibility(!bVertexOccluded);

	if (!bVertexOccluded)
	{
		UModumateFunctionLibrary::ComponentToUIScreenPosition(MeshComp, MoiLocation, FVector::ZeroVector);
		UModumateFunctionLibrary::ComponentAsBillboard(MeshComp, FVector(HandleScreenSize));
	}
}
