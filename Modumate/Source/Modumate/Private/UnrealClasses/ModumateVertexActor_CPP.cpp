// Copyright 2019 Modumate, Inc. All Rights Reserved.


#include "UnrealClasses/ModumateVertexActor_CPP.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "DocumentManagement/ModumateObjectInstanceMetaVertex.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

// Sets default values
AModumateVertexActor_CPP::AModumateVertexActor_CPP()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// Postupdate tick group to reduce jitter when updating position
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	
	SetMobility(EComponentMobility::Movable);
}

// Called when the game starts or when spawned
void AModumateVertexActor_CPP::BeginPlay()
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
void AModumateVertexActor_CPP::Tick(float DeltaTime)
{
	FVector newCameraLoc = Controller->PlayerCameraManager->GetCameraLocation();
	if (!CameraLocation.Equals(newCameraLoc))
	{
		CameraLocation = newCameraLoc;

		UpdateVisuals();
	}
}

void AModumateVertexActor_CPP::SetActorMesh(UStaticMesh *mesh)
{
	if (GetStaticMeshComponent() != nullptr)
	{
		MeshComp = GetStaticMeshComponent();
		MeshComp->SetStaticMesh(mesh);
		DynamicMaterial = UMaterialInstanceDynamic::Create(mesh->GetMaterial(0), this);
		MeshComp->SetMaterial(0, DynamicMaterial);
	}
}

void AModumateVertexActor_CPP::SetHandleScaleScreenSize(float NewSize)
{
	HandleScreenSize = NewSize;
	UpdateVisuals();
}

void AModumateVertexActor_CPP::SetMOILocation(const FVector &NewMOILocation)
{
	if (!MoiLocation.Equals(NewMOILocation, 0.0f))
	{
		MoiLocation = NewMOILocation;
		UpdateVisuals();
	}
}

void AModumateVertexActor_CPP::UpdateVisuals()
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
