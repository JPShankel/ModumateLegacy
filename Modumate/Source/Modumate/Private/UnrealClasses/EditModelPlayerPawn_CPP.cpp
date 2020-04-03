// Fill out your copyright notice in the Description page of Project Settings.

#include "EditModelPlayerPawn_CPP.h"

#include "CollisionShape.h"
#include "ModumateObjectEnums.h"

// Sets default values
AEditModelPlayerPawn_CPP::AEditModelPlayerPawn_CPP()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CachedCollisionObjQueryParams = FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects);
	CachedCollisionObjQueryParams.RemoveObjectTypesToQuery(COLLISION_HANDLE);

	static const FName ObjectTraceTag(TEXT("CollisionTrace"));
	CachedCollisionQueryParams = FCollisionQueryParams(ObjectTraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerPawn), false);
	CachedCollisionQueryParams.AddIgnoredActor(this);

	static const FName ObjectComplexTraceTag(TEXT("CollisionComplexTrace"));
	CachedCollisionQueryComplexParams = FCollisionQueryParams(ObjectComplexTraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerPawn), true);
	CachedCollisionQueryComplexParams.AddIgnoredActor(this);
}

// Called when the game starts or when spawned
void AEditModelPlayerPawn_CPP::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AEditModelPlayerPawn_CPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AEditModelPlayerPawn_CPP::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

bool AEditModelPlayerPawn_CPP::SphereTraceForZoomLocation(const FVector &Start, const FVector &End, float Radius, FHitResult& OutHit)
{
	return GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, CachedCollisionObjQueryParams, FCollisionShape::MakeSphere(Radius), CachedCollisionQueryParams);
}

bool AEditModelPlayerPawn_CPP::LineTraceForCollisionLocation(const FVector &Start, const FVector &End, FHitResult& OutHit, bool TraceComplex)
{
	if (TraceComplex)
	{
		return GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, CachedCollisionObjQueryParams, CachedCollisionQueryComplexParams);
	}
	else
	{
		return GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, CachedCollisionObjQueryParams, CachedCollisionQueryParams);
	}
}

bool AEditModelPlayerPawn_CPP::SetCameraFOV(float NewFOV)
{
	if (UCameraComponent *editCameraComponent = GetEditCameraComponent())
	{
		editCameraComponent->SetFieldOfView(NewFOV);
		return true;
	}

	return false;
}

bool AEditModelPlayerPawn_CPP::SetCameraTransform(const FTransform &PlayerActorTransform, const FTransform &CameraTransform, const FRotator &ControlRotation)
{
	if (GetEditCameraComponent() == nullptr)
	{
		return false;
	}

	SetActorTransform(PlayerActorTransform);
	GetEditCameraComponent()->SetWorldTransform(CameraTransform);
	Controller->SetControlRotation(ControlRotation);

	return true;
}
