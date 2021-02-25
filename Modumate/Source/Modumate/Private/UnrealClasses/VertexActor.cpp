// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/VertexActor.h"

#include "Engine/StaticMesh.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateDocument.h"

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

		// Disable shadows because they are expensive, and there are a lot of these things
		MeshComp->SetCastShadow(false);
	}

	Controller = Cast<AEditModelPlayerController>(this->GetWorld()->GetFirstPlayerController());

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
	// If there's a culling cutplane, check if this vertex is on the correct side first
	if (Controller->CurrentCullingCutPlaneID != MOD_ID_NONE)
	{
		const AModumateObjectInstance* cutPlaneMoi = Controller->GetDocument()->GetObjectById(Controller->CurrentCullingCutPlaneID);
		if (cutPlaneMoi && cutPlaneMoi->GetObjectType() == EObjectType::OTCutPlane)
		{
			FPlane cutPlaneCheck = FPlane(cutPlaneMoi->GetLocation(), cutPlaneMoi->GetNormal());
			if (cutPlaneCheck.PlaneDot(MoiLocation) < PLANAR_DOT_EPSILON)
			{
				MeshComp->SetVisibility(false);
				MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				return;
			}
		}
	}

	// If this vertex is not cull by the cutplane, check if it's being occluded by objects
	FHitResult hitResult;
	FVector dirToCamera = (CameraLocation - MoiLocation).GetSafeNormal();
	static float occlusionEpsilon = 0.1f;
	FVector occlusionTraceStart = MoiLocation + (occlusionEpsilon * dirToCamera);
	static float cameraBehindDist = 1000.0f;
	FVector occlusionTraceEnd = CameraLocation + (dirToCamera * cameraBehindDist);

	bool bVertexOccluded = Controller->LineTraceSingleAgainstMOIs(hitResult, occlusionTraceStart, occlusionTraceEnd);
	bool bValidVertex = !bVertexOccluded;
	if (bValidVertex)
	{
		bool bValidScreenPosition = UModumateFunctionLibrary::ComponentToUIScreenPosition(MeshComp, MoiLocation, FVector::ZeroVector);
		bool bValidBillboardRot = UModumateFunctionLibrary::ComponentAsBillboard(MeshComp, FVector(HandleScreenSize));
		bValidVertex = bValidScreenPosition && bValidBillboardRot;
	}

	MeshComp->SetVisibility(bValidVertex);
	MeshComp->SetCollisionEnabled(bValidVertex ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}
