// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/EditModelPlayerPawn_CPP.h"

#include "CollisionShape.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Database/ModumateObjectEnums.h"

// Sets default values
AEditModelPlayerPawn_CPP::AEditModelPlayerPawn_CPP(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EMPlayerController(nullptr)
	, bHaveEverBeenPossessed(false)
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

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(RootComponent);

	CameraCaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CameraCaptureComponent2D"));
	CameraCaptureComponent2D->SetupAttachment(RootComponent);
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

void AEditModelPlayerPawn_CPP::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	EMPlayerController = Cast<AEditModelPlayerController_CPP>(NewController);

	// Stop prioritizing axis input when we're possessed
	if (EMPlayerController && EMPlayerController->InputHandlerComponent && bHaveEverBeenPossessed)
	{
		EMPlayerController->InputHandlerComponent->RequestAxisInputPriority(StaticClass()->GetFName(), false);
	}

	bHaveEverBeenPossessed = true;
}

void AEditModelPlayerPawn_CPP::UnPossessed()
{
	// Start prioritizing axis input when we're not possessed
	if (EMPlayerController && EMPlayerController->InputHandlerComponent)
	{
		EMPlayerController->InputHandlerComponent->RequestAxisInputPriority(StaticClass()->GetFName(), true);
	}

	EMPlayerController = nullptr;

	Super::UnPossessed();
}

// Called to bind functionality to input
void AEditModelPlayerPawn_CPP::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (ensure(EMPlayerController && EMPlayerController->CameraController))
	{
		EMPlayerController->CameraController->SetupPlayerInputComponent(PlayerInputComponent);
	}
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
	if (CameraComponent)
	{
		CameraComponent->SetFieldOfView(NewFOV);
		return true;
	}

	return false;
}
