// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/EditModelPlayerPawn.h"

#include "CollisionShape.h"
#include "Components/Border.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Database/ModumateObjectEnums.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Online/ModumateClientIcon.h"
#include "UI/Online/OnlineUserName.h"
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
	RemoteMeshComponent->SetOwnerNoSee(true);

	CursorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CursorMeshComponent"));
	CursorMeshComponent->SetupAttachment(RootComponent);
	CursorMeshComponent->SetOwnerNoSee(true);
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

	UWorld* world = GetWorld();
	ULocalPlayer* localPlayer = world ? world->GetFirstLocalPlayerFromController() : nullptr;
	APlayerController* localController = localPlayer ? localPlayer->GetPlayerController(world) : nullptr;

	if (IsNetMode(NM_Client) && !IsLocallyControlled())
	{
		TryInitClientVisuals();

		if (ClientIconWidget)
		{
			FVector2D localViewportPos;
			if (localController->ProjectWorldLocationToScreen(GetActorLocation(), localViewportPos))
			{
				ClientIconWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				ClientIconWidget->SetPositionInViewport(localViewportPos, true);
			}
			else
			{
				ClientIconWidget->SetVisibility(ESlateVisibility::Hidden);
			}
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

bool AEditModelPlayerPawn::TryInitClientVisuals()
{
	UWorld* world = GetWorld();
	ULocalPlayer* localPlayer = world ? world->GetFirstLocalPlayerFromController() : nullptr;
	APlayerController* localController = localPlayer ? localPlayer->GetPlayerController(world) : nullptr;
	AEditModelPlayerHUD* localHUD = localController ? localController->GetHUD<AEditModelPlayerHUD>() : nullptr;
	AEditModelPlayerState* playerState = GetPlayerState<AEditModelPlayerState>();

	// Make sure that PlayerState has received all of the server-provided info required to initialize the client-side visuals
	if (!(localHUD && localHUD->HUDDrawWidget && ClientIconClass && playerState &&
		(playerState->MultiplayerClientIdx != INDEX_NONE) &&
		!playerState->ReplicatedUserInfo.Email.IsEmpty()))
	{
		return false;
	}

	FLinearColor playerColor = playerState->GetClientColor();

	if (ClientIconWidget == nullptr)
	{
		ClientIconWidget = localHUD->GetOrCreateWidgetInstance(ClientIconClass);
		if (!ensure(ClientIconWidget))
		{
			return false;
		}

		ClientIconWidget->AddToViewport();
		ClientIconWidget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));

		if (ClientIconWidget->ClientName->Background)
		{
			ClientIconWidget->ClientName->Background->SetBrushColor(playerColor);

			// Decide whether the text for the player name should be black or white, based on the luminance of the background,
			// for contrast purposes, based on standard accessibility thresholds.
			static const float whiteTextThresh = 0.66667f;
			float playerColorLuminance = playerColor.ComputeLuminance();
			FLinearColor textColor = (playerColorLuminance > whiteTextThresh) ? FLinearColor::Black : FLinearColor::White;
			ClientIconWidget->ClientName->ModumateTextBlock->SetColorAndOpacity(FSlateColor(textColor));
		}

		// User either the player's first name, or email if there's no first name, as the display name.
		const FString& playerName = !playerState->ReplicatedUserInfo.Firstname.IsEmpty() ?
			playerState->ReplicatedUserInfo.Firstname : playerState->ReplicatedUserInfo.Email;
		ClientIconWidget->ClientName->ChangeText(FText::FromString(playerName), false);
	}

	if (RemoteMeshMaterial == nullptr)
	{
		static const FName colorParamName(TEXT("ColorMultiplier"));
		static const int32 colorMaterialIdx = 1;
		RemoteMeshMaterial = UMaterialInstanceDynamic::Create(RemoteMeshComponent->GetMaterial(colorMaterialIdx), RemoteMeshComponent);
		if (!ensure(RemoteMeshMaterial))
		{
			return false;
		}

		RemoteMeshComponent->SetMaterial(colorMaterialIdx, RemoteMeshMaterial);
		RemoteMeshMaterial->SetVectorParameterValue(colorParamName, playerState->GetClientColor());

		// Share the same material with cursor mesh 
		if (CursorMeshComponent)
		{
			CursorMeshComponent->SetMaterial(0, RemoteMeshMaterial);
		}
	}

	return true;
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
