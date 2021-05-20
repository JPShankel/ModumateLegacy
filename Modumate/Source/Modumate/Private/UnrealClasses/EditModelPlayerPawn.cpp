// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/EditModelPlayerPawn.h"

#include "CollisionShape.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Database/ModumateObjectEnums.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Online/ModumateClientIcon.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

// Sets default values
AEditModelPlayerPawn::AEditModelPlayerPawn()
	: Super()
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

	ScreenshotTaker = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ScreenshotTaker"));
	ScreenshotTaker->SetupAttachment(RootComponent);

	RemoteMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RemoteMeshComponent"));
	RemoteMeshComponent->SetupAttachment(RootComponent);
	RemoteMeshComponent->SetVisibility(false);
}

// Called when the game starts or when spawned
void AEditModelPlayerPawn::BeginPlay()
{
	Super::BeginPlay();
}

void AEditModelPlayerPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ClientIconWidget)
	{
		ClientIconWidget->RemoveFromViewport();
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void AEditModelPlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsNetMode(NM_Client) && !IsLocallyControlled() && ClientIconWidget)
	{
		UWorld* world = GetWorld();
		ULocalPlayer* localPlayer = world ? world->GetFirstLocalPlayerFromController() : nullptr;
		APlayerController* localController = localPlayer ? localPlayer->GetPlayerController(world) : nullptr;
		FVector2D localViewportPos;

		if (localController && localController->ProjectWorldLocationToScreen(GetActorLocation(), localViewportPos))
		{
			ClientIconWidget->SetPositionInViewport(localViewportPos, true);
		}
	}
}

void AEditModelPlayerPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Stop prioritizing axis input when we're possessed
	auto emPlayerController = Cast<AEditModelPlayerController>(NewController);
	if (emPlayerController && emPlayerController->InputHandlerComponent && bHaveEverBeenPossessed)
	{
		emPlayerController->InputHandlerComponent->RequestAxisInputPriority(StaticClass()->GetFName(), false);
	}

	bHaveEverBeenPossessed = true;
}

void AEditModelPlayerPawn::UnPossessed()
{
	// Start prioritizing axis input when we're not possessed
	auto emPlayerController = Cast<AEditModelPlayerController>(Controller);
	if (emPlayerController && emPlayerController->InputHandlerComponent)
	{
		emPlayerController->InputHandlerComponent->RequestAxisInputPriority(StaticClass()->GetFName(), true);
	}

	emPlayerController = nullptr;

	Super::UnPossessed();
}

void AEditModelPlayerPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	AEditModelPlayerState* playerState = GetPlayerState<AEditModelPlayerState>();

	if (IsNetMode(NM_Client) && !IsLocallyControlled() && playerState)
	{
		UWorld* world = GetWorld();
		ULocalPlayer* localPlayer = world ? world->GetFirstLocalPlayerFromController() : nullptr;
		APlayerController* localController = localPlayer ? localPlayer->GetPlayerController(world) : nullptr;
		AEditModelPlayerHUD* localHUD = localController ? localController->GetHUD<AEditModelPlayerHUD>() : nullptr;
		ClientIconWidget = localHUD ? localHUD->GetOrCreateWidgetInstance(ClientIconClass) : nullptr;
		if (ensure(ClientIconWidget))
		{
			ClientIconWidget->AddToViewport();
			ClientIconWidget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));

			// Clear the name for now, since the player name will change after the client sends updated user info.
			ClientIconWidget->ClientName->ChangeText(FText::GetEmpty(), false);
		}

		RemoteMeshComponent->SetVisibility(true);
	}
}

// Called to bind functionality to input
void AEditModelPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	auto emPlayerController = Cast<AEditModelPlayerController>(Controller);
	if (ensure(emPlayerController && emPlayerController->CameraController))
	{
		emPlayerController->CameraController->SetupPlayerInputComponent(PlayerInputComponent);
	}
}

bool AEditModelPlayerPawn::SphereTraceForZoomLocation(const FVector &Start, const FVector &End, float Radius, FHitResult& OutHit)
{
	return GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, CachedCollisionObjQueryParams, FCollisionShape::MakeSphere(Radius), CachedCollisionQueryParams);
}

bool AEditModelPlayerPawn::LineTraceForCollisionLocation(const FVector &Start, const FVector &End, FHitResult& OutHit, bool TraceComplex)
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

bool AEditModelPlayerPawn::SetCameraFOV(float NewFOV)
{
	if (CameraComponent)
	{
		CameraComponent->SetFieldOfView(NewFOV);
		return true;
	}

	return false;
}
